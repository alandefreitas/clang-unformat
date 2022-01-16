#include <boost/process.hpp>
#include <boost/program_options.hpp>

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <futures/futures.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace fs = std::filesystem;
namespace process = boost::process;

/// \brief Levenshtein distance between two strings
inline std::size_t levenshtein_distance(std::string_view s1, std::string_view s2) {
    // trivial cases
    const std::size_t m(s1.size());
    const std::size_t n(s2.size());
    if (m == 0) {
        return n;
    }
    if (n == 0) {
        return m;
    }

    // init costs
    std::vector<std::size_t> costs(n + 1);
    for (std::size_t k = 0; k <= n; k++) {
        costs[k] = k;
    }

    // iterate s1
    std::size_t i{0};
    for (char const &c1 : s1) {
        costs[0] = i + 1;
        std::size_t corner{i};
        std::size_t j{0};
        for (char const &c2 : s2) {
            std::size_t upper{costs[j + 1]};
            if (c1 == c2) {
                costs[j + 1] = corner;
            } else {
                std::size_t t(upper < corner ? upper : corner);
                costs[j + 1] = (costs[j] < t ? costs[j] : t) + 1;
            }
            corner = upper;
            j++;
        }
        i++;
    }
    return costs[n];
}

inline std::size_t levenshtein_distance(const fs::path &p1, const fs::path &p2) {
    std::ifstream f1(p1);
    std::ifstream f2(p2);
    std::string s1((std::istreambuf_iterator<char>(f1)), std::istreambuf_iterator<char>());
    std::string s2((std::istreambuf_iterator<char>(f2)), std::istreambuf_iterator<char>());
    return levenshtein_distance(std::string_view(s1), std::string_view(s2));
}

inline void print_help(const boost::program_options::options_description &desc) { fmt::print("{}\n", desc); }

struct cli_config {
    bool help{false};
    fs::path input;
    fs::path output;
    fs::path temp;
    fs::path clang_format;
    std::vector<std::string> extensions;
    std::size_t parallel{futures::hardware_concurrency()};
    bool require_influence{false};
};

inline std::pair<boost::program_options::options_description, cli_config> parse_cli(int argc, char **argv) {
    namespace po = boost::program_options;
    po::options_description desc("clang-unformat");
    // clang-format off
    desc.add_options()
        ("help", "produce help message")
        ("input", po::value<fs::path>()->default_value(""), "input directory with source files")
        ("output", po::value<fs::path>()->default_value(""), "output path for the clang-format file")
        ("temp", po::value<fs::path>()->default_value(""), "temporary directory to formatted source files")
        ("clang-format", po::value<fs::path>()->default_value(""), "path to the clang-format executable")
        ("parallel", po::value<std::size_t>()->default_value(futures::hardware_concurrency()), "number of threads")
        ("require-influence", po::value<bool>()->default_value(false), "only include parameters that influence the output")
        ("extensions", po::value<std::vector<std::string>>(), "file extensions to format");
    // clang-format on
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    cli_config c;
    c.help = vm.count("help");
    c.input = vm["input"].as<fs::path>();
    c.temp = vm["temp"].as<fs::path>();
    c.clang_format = vm["clang-format"].as<fs::path>();
    if (vm.count("extensions")) {
        c.extensions = vm["extensions"].as<std::vector<std::string>>();
    }
    c.parallel = vm["parallel"].as<std::size_t>();
    c.require_influence = vm["require-influence"].as<bool>();
    return std::make_pair(desc, c);
}

inline bool equal_directory_layout(const fs::path &temp, const fs::path &input) {
    auto begin = fs::recursive_directory_iterator(input);
    auto end = fs::recursive_directory_iterator{};
    for (auto it = begin; it != end; ++it) {
        fs::path input_relative = fs::relative(*it, input);
        fs::path output_relative = temp / input_relative;
        if (!fs::exists(output_relative)) {
            return false;
        }
    }
    return true;
}

inline bool equal_subdirectory_layout(const fs::path &temp, const fs::path &input) {
    auto begin = fs::directory_iterator(temp);
    auto end = fs::directory_iterator{};
    for (auto it = begin; it != end; ++it) {
        auto const &p = *it;
        if (!fs::is_directory(p) || !equal_directory_layout(p, input)) {
            return false;
        }
    }
    return true;
}

inline bool validate_input(cli_config &config) {
    namespace fs = std::filesystem;
    fmt::print(fmt::fg(fmt::terminal_color::blue), "## Validating input\n");
    if (config.input.empty()) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Input directory not provided\n");
        return false;
    }
    if (!fs::exists(config.input)) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Input {} is does not exist", config.input);
        return false;
    }
    if (!fs::is_directory(config.input)) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Input {} is not a directory", config.input);
        return false;
    }
    fmt::print(fmt::fg(fmt::terminal_color::green), "config \"input\" {} OK!\n", config.input);
    fmt::print("\n");

    fmt::print(fmt::fg(fmt::terminal_color::blue), "## Validating output\n");
    if (config.output.empty()) {
        fmt::print("No output path set\n");
        config.output = config.input / ".clang-format";
        fmt::print(fmt::fg(fmt::terminal_color::yellow), "output path set to {}\n", config.output);
    }
    if (fs::exists(config.output)) {
        fmt::print("output path {} already exists\n", config.output);
        if (fs::is_directory(config.output)) {
            fmt::print(fmt::fg(fmt::terminal_color::yellow), "output {} is a directory\n", config.output);
            config.output /= ".clang-format";
            fmt::print(fmt::fg(fmt::terminal_color::yellow), "output set to {}\n", config.output);
            return false;
        }
    }
    if (config.output.filename() != ".clang-format") {
        fmt::print(fmt::fg(fmt::terminal_color::red), "output file {} should be .clang-format\n", config.output);
        return false;
    }
    if (!fs::exists(config.output)) {
        fmt::print(fmt::fg(fmt::terminal_color::blue), "output file {} doesn't exist yet\n", config.output);
    }
    fmt::print(fmt::fg(fmt::terminal_color::green), "config \"output\" {} OK!\n", config.output);
    fmt::print("\n");

    fmt::print(fmt::fg(fmt::terminal_color::blue), "## Validating temp\n");
    if (config.temp.empty()) {
        fmt::print("No temp directory set\n");
        config.temp = fs::current_path() / "clang-unformat-temp";
        fmt::print(fmt::fg(fmt::terminal_color::yellow), "temp directory set to {}\n", config.temp);
    }
    if (fs::exists(config.temp)) {
        fmt::print(fmt::fg(fmt::terminal_color::yellow), "temp directory {} already exists\n", config.temp);
        if (!fs::is_directory(config.temp)) {
            fmt::print(fmt::fg(fmt::terminal_color::red), "temp {} is not a directory\n", config.temp);
            return false;
        }
        auto n_files = std::distance(fs::directory_iterator(config.temp), fs::directory_iterator{});
        if (n_files) {
            if (equal_directory_layout(config.temp, config.input) ||
                equal_subdirectory_layout(config.temp, config.input)) {
                fmt::print("temp directory {} is not empty but has an valid directory layout\n", config.temp);
            } else {
                fmt::print(fmt::fg(fmt::terminal_color::red), "temp directory {} cannot be used\n", config.temp);
                return false;
            }
        }
    } else {
        fs::create_directories(config.temp);
        fmt::print(fmt::fg(fmt::terminal_color::green), "temp directory {} created\n", config.temp);
    }
    fmt::print(fmt::fg(fmt::terminal_color::green), "config \"temp\" {} OK!\n", config.temp);
    fmt::print("\n");

    fmt::print(fmt::fg(fmt::terminal_color::blue), "## Validating clang-format\n");
    if (config.clang_format.empty()) {
        fmt::print("no clang-format path set\n");
        config.clang_format = process::search_path("clang-format").c_str();
        if (fs::exists(config.clang_format)) {
            fmt::print(fmt::fg(fmt::terminal_color::yellow), "default to {}\n", config.clang_format);
            process::ipstream is;
            process::child c(config.clang_format.c_str(), "--version", process::std_out > is);
            std::string line;
            while (c.running() && std::getline(is, line) && !line.empty()) {
                fmt::print(fmt::fg(fmt::terminal_color::green), "{}\n", line);
                std::string_view line_view(line);
                if (line_view.substr(0, 21) == "clang-format version ") {
                    auto major_end = line_view.find_first_of('.', 21);
                    if (major_end != std::string_view::npos) {
                        auto clang_version_view = line_view.substr(21, major_end - 21);
                        std::size_t major = 0;
                        auto res = std::from_chars(clang_version_view.data(),
                                                   clang_version_view.data() + clang_version_view.size(), major, 10);
                        if (res.ec == std::errc{} && major < 13) {
                            fmt::print(fmt::fg(fmt::terminal_color::red),
                                       "You need to update clang-format from {} for this to work properly\n", major);
                        }
                    }
                }
            }
        } else {
            fmt::print(fmt::fg(fmt::terminal_color::red), "cannot find clang-format in PATH\n");
            return false;
        }
    } else if (!fs::exists(config.clang_format)) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "cannot find clang-format in path {}\n", config.clang_format);
        return false;
    }
    fmt::print(fmt::fg(fmt::terminal_color::green), "config \"clang_format\" {} OK!\n", config.clang_format);
    fmt::print("\n");

    fmt::print(fmt::fg(fmt::terminal_color::blue), "## Validating file extensions\n");
    if (config.extensions.empty()) {
        fmt::print("no file extensions set\n");
        config.extensions = {"h", "hpp", "cpp", "ipp"};
        fmt::print(fmt::fg(fmt::terminal_color::yellow), "default to: {}\n", config.extensions);
    }
    fmt::print(fmt::fg(fmt::terminal_color::green), "config \"extensions\" {} OK!\n", config.extensions);
    fmt::print("\n");

    fmt::print(fmt::fg(fmt::terminal_color::blue), "## Validating threads\n");
    if (config.parallel == 0) {
        fmt::print(fmt::fg(fmt::terminal_color::yellow), "Cannot execute with {} threads\n", config.parallel);
        config.parallel = futures::hardware_concurrency();
        fmt::print(fmt::fg(fmt::terminal_color::yellow), "Defaulting to {} threads\n", config.parallel);
    }
    fmt::print(fmt::fg(fmt::terminal_color::green), "config \"parallel\" {} OK!\n", config.parallel);
    fmt::print("\n");

    fmt::print("=============================\n\n");
    return true;
}

struct clang_format_possible_values {
    clang_format_possible_values(std::initializer_list<std::string_view> opts) : options(opts.begin(), opts.end()) {}

    // Values we can use in this parameter
    std::vector<std::string> options;

    // A required value another parameter needs for this parameter to work
    std::pair<std::string, std::string> requirements;

    // Prefix from which we should take the default values if this doesn't affect the output
    std::string default_value_from_prefix;

    // Default value if there is no prefix and this doesn't affect the output
    std::string default_value;
};

inline bool starts_with(std::string_view str, std::string_view substr) {
    return str.substr(0, substr.size()) == substr;
}
inline bool ends_with(std::string_view str, std::string_view substr) {
    return str.size() > substr.size() && str.substr(str.size() - substr.size(), substr.size()) == substr;
}

inline std::vector<std::pair<std::string, clang_format_possible_values>> generate_clang_format_options() {
    std::vector<std::pair<std::string, clang_format_possible_values>> result{
        // Language, this format style is targeted at.
        {"Language", {"Cpp"}},
        // The style used for all options not specifically set
        {"BasedOnStyle", {"LLVM", "Google", "Chromium", "Mozilla", "WebKit", "Microsoft", "GNU"}},
        // The extra indent or outdent of access modifiers
        {"AccessModifierOffset",
         {"-8", "-6", "-4", "-2", "0", "2", "4", "6", "8"}}, // horizontally aligns arguments after an open bracket
        {"AlignAfterOpenBracket", {"BAS_Align", "BAS_DontAlign", "BAS_AlwaysBreak"}},
        // when using initialization for an array of structs aligns the fields into columns
        {"AlignArrayOfStructures", {"AIAS_Left", "AIAS_Right", "AIAS_None"}},
        // Style of aligning consecutive assignments
        {"AlignConsecutiveAssignments",
         {"ACS_None", "ACS_Consecutive", "ACS_AcrossEmptyLines", "ACS_AcrossComments",
          "ACS_AcrossEmptyLinesAndComments"}},
        // Style of aligning consecutive bit field
        {"AlignConsecutiveBitFields",
         {"ACS_None", "ACS_Consecutive", "ACS_AcrossEmptyLines", "ACS_AcrossComments",
          "ACS_AcrossEmptyLinesAndComments"}},
        // Style of aligning consecutive declarations
        {"AlignConsecutiveDeclarations",
         {"ACS_None", "ACS_Consecutive", "ACS_AcrossEmptyLines", "ACS_AcrossComments",
          "ACS_AcrossEmptyLinesAndComments"}},
        // Style of aligning consecutive declarations
        {"AlignConsecutiveMacros",
         {"ACS_None", "ACS_Consecutive", "ACS_AcrossEmptyLines", "ACS_AcrossComments",
          "ACS_AcrossEmptyLinesAndComments"}},
        // Options for aligning backslashes in escaped newlines
        {"AlignEscapedNewlines", {"ENAS_DontAlign", "ENAS_Left", "ENAS_Right"}},
        // horizontally align operands of binary and ternary expressions
        {"AlignOperands", {"OAS_DontAlign", "OAS_Align", "OAS_AlignAfterOperator"}},
        // aligns trailing comments
        {"AlignTrailingComments", {"true", "false"}},
        // allow putting all arguments onto the next line
        {"AllowAllArgumentsOnNextLine", {"true", "false"}},
        // putting all parameters of a function declaration onto the next line
        {"AllowAllParametersOfDeclarationOnNextLine", {"true", "false"}},
        // "while (true) { continue }" can be put on a single line
        {"AllowShortBlocksOnASingleLine", {"SBS_Never", "SBS_Empty", "SBS_Always"}},
        // short case labels will be contracted to a single line.
        {"AllowShortCaseLabelsOnASingleLine", {"true", "false"}},
        // Allow short enums on a single line.
        {"AllowShortEnumsOnASingleLine", {"true", "false"}},
        // int f() { return 0; } can be put on a single line
        {"AllowShortFunctionsOnASingleLine", {"SFS_None", "SFS_InlineOnly", "SFS_Empty", "SFS_Inline", "SFS_All"}},
        // if (a) return can be put on a single line.
        {"AllowShortIfStatementsOnASingleLine",
         {"SIS_Never", "SIS_WithoutElse", "SIS_OnlyFirstIf", "SIS_AllIfsAndElse"}},
        // auto lambda []() { return 0; } can be put on a single line.
        {"AllowShortLambdasOnASingleLine", {"SLS_None", "SLS_Empty", "SLS_Inline", "SLS_All"}},
        // If true, while (true) continue can be put on a single line.
        {"AllowShortLoopsOnASingleLine", {"true", "false"}},
        // The function definition return type breaking style to use.
        {"AlwaysBreakAfterDefinitionReturnType", {"DRTBS_None", "DRTBS_All", "DRTBS_TopLevel"}},
        // The function declaration return type breaking style to use.
        {"AlwaysBreakAfterReturnType",
         {"RTBS_None", "RTBS_All", "RTBS_TopLevel", "RTBS_AllDefinitions", "RTBS_TopLevelDefinitions"}},
        // If true, always break before multiline string literals.
        {"AlwaysBreakBeforeMultilineStrings", {"true", "false"}},
        // The template declaration breaking style to use.
        {"AlwaysBreakTemplateDeclarations", {"BTDS_No", "BTDS_MultiLine", "BTDS_Yes"}},
        // If false, a function call’s arguments will either be all on the same line or will have one line each.
        {"BinPackArguments", {"true", "false"}},
        // If false, a function declaration’s or function definition’s parameters will either all be on the same
        // line or
        // will have one line each.
        {"BinPackParameters", {"true", "false"}},
        // The BitFieldColonSpacingStyle to use for bitfields.
        {"BitFieldColonSpacing", {"BFCS_Both", "BFCS_None", "BFCS_Before", "BFCS_After"}},
        // The brace breaking style to use.
        {"BreakBeforeBraces",
         {"BS_Attach", "BS_Linux", "BS_Mozilla", "BS_Stroustrup", "BS_Allman", "BS_Whitesmiths", "BS_GNU", "BS_WebKit",
          "BS_Custom"}},
        // Control of individual brace wrapping cases.
        {"BraceWrapping.AfterCaseLabel", {"true", "false"}},
        {"BraceWrapping.AfterClass", {"true", "false"}},
        {"BraceWrapping.AfterControlStatement", {"BWACS_Never", "BWACS_MultiLine", "BWACS_Always"}},
        {"BraceWrapping.AfterEnum", {"true", "false"}},
        {"BraceWrapping.AfterFunction", {"true", "false"}},
        {"BraceWrapping.AfterNamespace", {"true", "false"}},
        {"BraceWrapping.AfterObjCDeclaration", {"true", "false"}},
        {"BraceWrapping.AfterStruct", {"true", "false"}},
        {"BraceWrapping.AfterUnion", {"true", "false"}},
        {"BraceWrapping.AfterExternBlock", {"true", "false"}},
        {"BraceWrapping.BeforeCatch", {"true", "false"}},
        {"BraceWrapping.BeforeElse", {"true", "false"}},
        {"BraceWrapping.BeforeLambdaBody", {"true", "false"}},
        {"BraceWrapping.BeforeWhile", {"true", "false"}},
        {"BraceWrapping.IndentBraces", {"true", "false"}},
        {"BraceWrapping.SplitEmptyFunction", {"true", "false"}},
        {"BraceWrapping.SplitEmptyRecord", {"true", "false"}},
        {"BraceWrapping.SplitEmptyNamespace", {"true", "false"}},
        // Break after each annotation on a field in Java files.
        {"BreakAfterJavaFieldAnnotations", {"true", "false"}},
        // The way to wrap binary operators.
        {"BreakBeforeBinaryOperators", {"BOS_None", "BOS_NonAssignment", "BOS_All"}},
        // If true, concept will be placed on a new line.
        {"BreakBeforeConceptDeclarations", {"true", "false"}},
        // If true, ternary operators will be placed after line breaks.
        {"BreakBeforeTernaryOperators", {"true", "false"}},
        // The break constructor initializers style to use.
        {"BreakConstructorInitializers", {"BCIS_BeforeColon", "BCIS_BeforeComma", "BCIS_AfterColon"}},
        // The inheritance list style to use.
        {"BreakInheritanceList", {"BILS_BeforeColon", "BILS_BeforeComma", "BILS_AfterColon", "BILS_AfterComma"}},
        // Allow breaking string literals when formatting.
        {"BreakStringLiterals", {"true", "false"}},
        // The column limit
        {"ColumnLimit", {"40", "60", "80", "100", "120", "140"}},
        // If true, consecutive namespace declarations will be on the same line. If false, each namespace is
        // declared on
        // a new line.
        {"CompactNamespaces", {"true", "false"}},
        // The number of characters to use for indentation of constructor initializer lists as well as inheritance
        // lists.
        {"ConstructorInitializerIndentWidth", {"0", "2", "4", "6", "8", "10", "12"}},
        // Indent width for line continuations.
        {"ContinuationIndentWidth", {"0", "2", "4", "6", "8", "10"}},
        // If true, format braced lists as best suited for C++11 braced lists..
        {"Cpp11BracedListStyle", {"true", "false"}},
        // Analyze the formatted file for the most used line ending (\r\n or \n). UseCRLF is only used as a fallback
        // if
        // none can be derived.
        {"DeriveLineEnding", {"true", "false"}},
        // If true, analyze the formatted file for the most common alignment of & and *.
        {"DerivePointerAlignment", {"true", "false"}},
        // Defines when to put an empty line after access modifiers.
        {"EmptyLineAfterAccessModifier", {"ELAAMS_Never", "ELAAMS_Leave", "ELAAMS_Always"}},
        // Defines in which cases to put empty line before access modifiers.
        {"EmptyLineBeforeAccessModifier", {"ELBAMS_Never", "ELBAMS_Leave", "ELBAMS_LogicalBlock", "ELBAMS_Always"}},
        // If true, clang-format detects whether function calls and definitions are formatted with one parameter per
        // line.
        {"ExperimentalAutoDetectBinPacking", {"true", "false"}},
        // If true, clang-format adds missing namespace end comments for short namespaces and fixes invalid existing
        // ones..
        {"FixNamespaceComments", {"true", "false"}},
        // Dependent on the value, multiple #include blocks can be sorted as one and divided based on category.
        {"IncludeBlocks", {"IBS_Preserve", "IBS_Merge", "IBS_Regroup"}},
        // Specify whether access modifiers should have their own indentation level.
        {"IndentAccessModifiers", {"true", "false"}},
        // Indent case label blocks one level from the case label.
        {"IndentCaseBlocks", {"true", "false"}},
        // Indent case labels one level from the switch statement.
        {"IndentCaseLabels", {"true", "false"}},
        // IndentExternBlockStyle is the type of indenting of extern blocks..
        {"IndentExternBlock", {"IEBS_AfterExternBlock", "IEBS_NoIndent", "IEBS_Indent"}},
        // Indent goto labels.
        {"IndentGotoLabels", {"true", "false"}},
        // The preprocessor directive indenting style to use.
        {"IndentPPDirectives", {"PPDIS_None", "PPDIS_AfterHash", "PPDIS_BeforeHash"}},
        // Indent the requires clause in a template.
        {"IndentRequires", {"true", "false"}},
        // The number of columns to use for indentation..
        {"IndentWidth", {"0", "2", "4", "6", "8"}},
        // Indent if a function definition or declaration is wrapped after the type.
        {"IndentWrappedFunctionNames", {"true", "false"}},
        // Indent if a function definition or declaration is wrapped after the type.
        {"InsertTrailingCommas", {"TCS_None", "TCS_Wrapped"}},
        // If true, the empty line at the start of blocks is kept.
        {"KeepEmptyLinesAtTheStartOfBlocks", {"true", "false"}},
        // The indentation style of lambda bodies.
        {"LambdaBodyIndentation", {"LBI_Signature", "LBI_OuterScope"}},
        // The maximum number of consecutive empty lines to keep.
        {"MaxEmptyLinesToKeep", {"0", "2", "4", "8", "16"}},
        // The indentation used for namespaces.
        {"NamespaceIndentation", {"NI_None", "NI_Inner", "NI_All"}},
        // The penalty for breaking around an assignment operator.
        {"PenaltyBreakAssignment", {"2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048"}},
        // The penalty for breaking a function call after call
        {"PenaltyBreakBeforeFirstCallParameter",
         {"2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048"}},
        // The penalty for each line break introduced inside a comment.
        {"PenaltyBreakComment", {"2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048"}},
        // The penalty for breaking before the first <<.
        {"PenaltyBreakFirstLessLess", {"2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048"}},
        // The penalty for each line break introduced inside a string literal.
        {"PenaltyBreakString", {"2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048"}},
        // The penalty for breaking after template declaration.
        {"PenaltyBreakTemplateDeclaration", {"2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048"}},
        // The penalty for each character outside of the column limit.
        {"PenaltyExcessCharacter", {"2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048"}},
        // Penalty for each character of whitespace indentation (counted relative to leading non-whitespace column).
        {"PenaltyIndentedWhitespace", {"2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048"}},
        // Penalty for putting the return type of a function onto its own line.
        {"PenaltyReturnTypeOnItsOwnLine", {"2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048"}},
        // Pointer and reference alignment style.
        {"PointerAlignment", {"PAS_Left", "PAS_Right", "PAS_Middle"}},
        // Reference alignment style (overrides PointerAlignment for references).
        {"ReferenceAlignment", {"RAS_Pointer", "RAS_Left", "RAS_Right", "RAS_Middle"}},
        // If true, clang-format will attempt to re-flow comments.
        {"ReflowComments", {"true", "false"}},
        // The maximal number of unwrapped lines that a short namespace spans.
        {"ShortNamespaceLines", {"0", "1", "4", "8"}},
        // Controls if and how clang-format will sort #includes.
        {"SortIncludes", {"SI_Never", "SI_CaseSensitive", "SI_CaseInsensitive"}},
        // If true, clang-format will sort using declarations.
        {"SortUsingDeclarations", {"true", "false"}},
        // If true, a space is inserted after C style casts.
        {"SpaceAfterCStyleCast", {"true", "false"}},
        // If true, a space is inserted after the logical not operator (!).
        {"SpaceAfterLogicalNot", {"true", "false"}},
        // If true, a space will be inserted after the ‘template’ keyword.
        {"SpaceAfterTemplateKeyword", {"true", "false"}},
        // Defines in which cases to put a space before or after pointer qualifiers
        {"SpaceAroundPointerQualifiers", {"SAPQ_Default", "SAPQ_Before", "SAPQ_After", "SAPQ_Both"}},
        // If false, spaces will be removed before assignment operators.
        {"SpaceBeforeAssignmentOperators", {"true", "false"}},
        // If false, spaces will be removed before case colon.
        {"SpaceBeforeCaseColon", {"true", "false"}},
        // If true, a space will be inserted before a C++11 braced list used to initialize an object (after the
        // preceding identifier or type).
        {"SpaceBeforeCpp11BracedList", {"true", "false"}},
        // If false, spaces will be removed before constructor initializer colon.
        {"SpaceBeforeCtorInitializerColon", {"true", "false"}},
        // If false, spaces will be removed before inheritance colon.
        {"SpaceBeforeInheritanceColon", {"true", "false"}},
        // Defines in which cases to put a space before opening parentheses.
        {"SpaceBeforeParens",
         {"SBPO_Never", "SBPO_ControlStatements", "SBPO_ControlStatementsExceptControlMacros",
          "SBPO_NonEmptyParentheses", "SBPO_Always"}},
        // If false, spaces will be removed before range-based for loop colon.
        {"SpaceBeforeRangeBasedForLoopColon", {"true", "false"}},
        // If true, spaces will be before [. Lambdas will not be affected. Only the first [ will get a space added.
        {"SpaceBeforeSquareBrackets", {"true", "false"}},
        // If true, spaces will be inserted into {}.
        {"SpaceInEmptyBlock", {"true", "false"}},
        // If true, spaces may be inserted into ().
        {"SpaceInEmptyParentheses", {"true", "false"}},
        // The number of spaces before trailing line comments (// - comments).
        {"SpacesBeforeTrailingComments", {"0", "1", "2", "4", "8"}},
        // The SpacesInAnglesStyle to use for template argument lists.
        {"SpacesInAngles", {"SIAS_Never", "SIAS_Always", "SIAS_Leave"}},
        // If true, spaces may be inserted into C style casts.
        {"SpacesInCStyleCastParentheses", {"true", "false"}},
        // If true, spaces will be inserted around if/for/switch/while conditions.
        {"SpacesInConditionalStatement", {"true", "false"}},
        // If true, spaces are inserted inside container literals (e.g. ObjC and Javascript array and dict
        // literals).
        {"SpacesInContainerLiterals", {"true", "false"}},
        // If true, spaces will be inserted after ( and before ).
        {"SpacesInParentheses", {"true", "false"}},
        // Parse and format C++ constructs compatible with this standard.
        {"Standard", {"c++03", "c++11", "c++14", "c++17", "c++20", "Latest", "Auto"}},
        // The number of columns used for tab stops.
        {"TabWidth", {"0", "2", "4", "6", "8"}},
        // Use \r\n instead of \n for line breaks. Also used as fallback if DeriveLineEnding is true.
        {"UseCRLF", {"true", "false"}},
        // The way to use tab characters in the resulting file.
        {"UseTab",
         {"UT_Never", "UT_ForIndentation", "UT_ForContinuationAndIndentation", "UT_AlignWithSpaces", "UT_Always"}},
    };

    // set requirements
    for (auto &[key, value] : result) {
        if (starts_with(key, "BraceWrapping.")) {
            value.requirements = {"BreakBeforeBraces", "BS_Custom"};
        }
    }

    // set default_value_from_prefix
    for (auto &[key, value] : result) {
        for (auto &prefix : {"Align", "AlwaysBreak", "BraceWrapping", "BreakBefore", "Derive", "EmptyLine", "Indent",
                             "PenaltyBreak", "SpaceAfter", "SpaceBefore", "SpaceIn"}) {
            if (starts_with(key, prefix)) {
                value.default_value_from_prefix = prefix;
            }
        }
    }

    // set default_value
    for (auto &[key, value] : result) {
        // clang-format off
        std::vector<std::pair<std::string, std::vector<std::string>>> prefix_defaults {
            {"Align", {"true", "Always", "Right", "Yes", "Consecutive"}},
            {"Allow", {"false", "Never", "None", "No"}},
            {"AlwaysBreak", {"true", "Always", "All", "Yes"}},
            {"BreakBefore", {"true", "Always", "All", "Yes"}},
            {"Derive", {"true", "Always", "All", "Yes"}},
            {"EmptyLine", {"true", "Always", "All", "Yes"}},
            {"Indent", {"true", "Always", "All", "Yes"}},
            {"PenaltyBreak", {"true", "Always", "All", "Yes"}},
            {"SpaceAfter", {"true", "Always", "All", "Yes"}},
            {"SpaceBefore", {"true", "Always", "All", "Yes"}},
            {"SpaceIn", {"true", "Always", "All", "Yes"}},
        };
        // clang-format on
        for (auto &[prefix, reasonable_defaults] : prefix_defaults) {
            if (starts_with(key, prefix)) {
                auto it = std::find_if(
                    reasonable_defaults.begin(), reasonable_defaults.end(), [value = value](std::string const &def) {
                        return std::find_if(value.options.begin(), value.options.end(), [&](const std::string &opt) {
                                   return ends_with(opt, def);
                               }) != value.options.end();
                    });
                if (it != reasonable_defaults.end()) {
                    value.default_value = *it;
                }
            }
        }
    }

    return result;
}

struct clang_format_entry {
    std::string key;
    std::string value;
    bool affected_output{true};
    std::size_t score{std::size_t(-1)};
    bool failed{false};
    std::string comment;
};

inline void print(const std::vector<clang_format_entry> &current_cf) {
    std::string prev_section;
    for (const auto &[key, value, affected_output, score, failed, comment] : current_cf) {
        // Key
        std::size_t key_width = 0;
        auto subsection_begin = key.find_first_of('.');
        if (bool section_only = subsection_begin == std::string::npos; section_only) {
            fmt::print(fmt::fg(fmt::terminal_color::green), "{}", key);
            key_width = key.size();
        } else {
            auto section = std::string_view(key).substr(0, subsection_begin);
            auto subsection = std::string_view(key).substr(subsection_begin + 1);
            if (prev_section != section) {
                fmt::print(fmt::fg(fmt::terminal_color::green), "{}:\n", section);
                prev_section = section;
            }
            fmt::print(fmt::fg(fmt::terminal_color::green), "  {}", subsection);
            key_width = subsection.size() + 2;
        }
        fmt::print(": ");

        // Value
        std::string_view value_view = value;
        if (auto last_under = value_view.find_last_of('_'); last_under != std::string_view::npos) {
            value_view = value_view.substr(last_under + 1);
        }
        fmt::print(fmt::fg(fmt::terminal_color::blue), "{}", value_view);
        std::size_t value_width = value_view.size();
        std::size_t entry_width = key_width + value_width + 2;
        std::size_t comment_padding = entry_width > 50 ? 0 : 50 - entry_width;
        if (!comment.empty()) {
            fmt::print("{1: ^{2}} # {0}", comment, "", comment_padding);
        } else if (failed) {
            fmt::print("{0: ^{1}} # parameter failed", "", comment_padding);
        } else if (!affected_output) {
            fmt::print("{0: ^{1}} # did not affect the output", "", comment_padding);
        } else if (score != 0) {
            fmt::print("{1: ^{2}} # edit distance {0}", score, "", comment_padding);
        }
        fmt::print("\n");
    }
}

inline void save(const std::vector<clang_format_entry> &current_cf, const fs::path &output) {
    std::ofstream fout(output);
    fout << "# .clang-format inferred with clang-unformat (https://www.github.com/alandefreitas/clang-unformat)\n";
    std::string prev_section;
    std::size_t key_width = 0;
    for (const auto &[key, value, affected_output, score, failed, comment] : current_cf) {
        auto subsection_begin = key.find_first_of('.');
        if (bool section_only = subsection_begin == std::string::npos; section_only) {
            fout << fmt::format("{}", key);
            key_width = key.size();
        } else {
            auto section = std::string_view(key).substr(0, subsection_begin);
            auto subsection = std::string_view(key).substr(subsection_begin + 1);
            if (prev_section != section) {
                fout << fmt::format("{}:\n", section);
                prev_section = section;
            }
            fout << fmt::format("  {}", subsection);
            key_width = subsection.size() + 2;
        }
        std::string_view value_view = value;
        auto last_under = value_view.find_last_of('_');
        if (bool needs_split = last_under != std::string_view::npos; needs_split) {
            value_view = value_view.substr(last_under + 1);
        }
        fout << fmt::format(": {}", value_view);
        std::size_t value_width = value_view.size();
        std::size_t entry_width = key_width + value_width + 2;
        std::size_t comment_padding = entry_width > 50 ? 0 : 50 - entry_width;
        if (!comment.empty()) {
            fout << fmt::format("{1: ^{2}} # {0}", comment, "", comment_padding);
        } else if (failed) {
            fout << fmt::format("{0: ^{1}} # parameter failed", "", comment_padding);
        } else if (!affected_output) {
            fout << fmt::format("{0: ^{1}} # did not affect the output", "", comment_padding);
        } else if (score != 0) {
            fout << fmt::format("{1: ^{2}} # edit distance {0}", score, "", comment_padding);
        }
        fout << fmt::format("\n");
    }
}

inline bool should_format(const cli_config &config, const fs::path &p) {
    if (fs::is_regular_file(p) && p.has_extension()) {
        auto file_ext = p.extension();
        auto file_ext_str = p.extension();
        std::string_view file_ext_view = file_ext_str.c_str();
        auto it = std::find_if(config.extensions.begin(), config.extensions.end(), [file_ext_view](auto ext) {
            return file_ext_view == ext || (file_ext_view.front() == '.' && file_ext_view.substr(1) == ext);
        });
        return it != config.extensions.end();
    }
    return false;
}

inline bool format_temp_directory(const cli_config &config, const fs::path &task_temp) {
    auto begin = fs::recursive_directory_iterator(task_temp);
    auto end = fs::recursive_directory_iterator{};
    for (auto it = begin; it != end; ++it) {
        fs::path p = it->path();
        if (should_format(config, p)) {
            process::ipstream is;
            process::child c(config.clang_format.c_str(), "-i", fs::absolute(p).c_str(), process::std_out > is);
            std::string line;
            bool first_error_line = true;
            while (c.running() && std::getline(is, line) && !line.empty()) {
                if (first_error_line) {
                    fmt::print(fmt::fg(fmt::terminal_color::red), "clang-format error!\n");
                    first_error_line = false;
                }
                fmt::print(fmt::fg(fmt::terminal_color::red), "{}\n", line);
            }
            c.wait();
            if (c.exit_code() != 0) {
                return false;
            }
        }
    }
    return true;
}

inline std::size_t distance_formatted_files(const cli_config &config, const fs::path &task_temp) {
    std::size_t total_distance = 0;
    auto begin = fs::recursive_directory_iterator(config.input);
    auto end = fs::recursive_directory_iterator{};
    for (auto it = begin; it != end; ++it) {
        fs::path p = it->path();
        if (should_format(config, p)) {
            fs::path input_relative = fs::relative(p, config.input);
            fs::path output_relative = task_temp / input_relative;
            total_distance += levenshtein_distance(p, output_relative);
        }
    }
    return total_distance;
}

inline std::size_t evaluate(const cli_config &config, const fs::path &task_temp) {
    if (!format_temp_directory(config, task_temp)) {
        return std::size_t(-1);
    }
    return distance_formatted_files(config, task_temp);
}

inline std::string pretty_time(std::chrono::steady_clock::duration d) {
    auto mc = std::chrono::duration_cast<std::chrono::microseconds>(d);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d);
    auto s = std::chrono::duration_cast<std::chrono::seconds>(d);
    auto m = std::chrono::duration_cast<std::chrono::minutes>(d);
    auto h = std::chrono::duration_cast<std::chrono::hours>(d);
    m = m - h;
    s = s - m - h;
    ms = ms - s - m - h;
    mc = mc - ms - s - m - h;
    if (h.count() > 1) {
        return fmt::format("{}:{}:{}", h, m, s);
    } else if (m.count() > 1) {
        return fmt::format("{}:{}:{}", m, s, ms);
    } else {
        return fmt::format("{}:{}:{}", s, ms, mc);
    }
}

void clang_format_local_search(const cli_config &config) {
    std::vector<std::pair<std::string, clang_format_possible_values>> cf_opts = generate_clang_format_options();
    std::vector<clang_format_entry> current_cf;
    std::chrono::steady_clock::duration total_evaluation_time(0);
    std::size_t total_neighbors =
        std::accumulate(cf_opts.begin(), cf_opts.end(), std::size_t(0),
                        [](std::size_t x, auto const &p) { return x + p.second.options.size(); });
    std::size_t total_neighbors_evaluated = 0;
    futures::asio::thread_pool pool(config.parallel);
    auto ex = pool.executor();

    for (const auto &[key, possible_values] : cf_opts) {
        // Apply requirements
        bool req_applied = false;
        clang_format_entry prev_entry;
        if (!possible_values.requirements.first.empty()) {
            auto &p = possible_values;
            auto current_cf_it =
                std::find_if(current_cf.begin(), current_cf.end(),
                             [&](clang_format_entry const &entry) { return entry.key == p.requirements.first; });
            if (current_cf_it != current_cf.end()) {
                req_applied = true;
                prev_entry = *current_cf_it;
                current_cf_it->value = p.requirements.second;
            }
        }

        if (!current_cf.empty() && total_evaluation_time > std::chrono::seconds(1)) {
            fmt::print("==============================\n");
            fmt::print(fmt::fg(fmt::terminal_color::yellow), "# Current .clang-format:\n");
            print(current_cf);
            fmt::print("# Total evaluation time: {}\n", pretty_time(total_evaluation_time));
            auto avg_evaluation_time = total_evaluation_time / total_neighbors_evaluated;
            fmt::print("# Average evaluation time: {} per parameter value\n", pretty_time(avg_evaluation_time));
            auto est_evaluation_time = avg_evaluation_time * (total_neighbors - total_neighbors_evaluated);
            fmt::print("# Estimated time left: {}\n", pretty_time(est_evaluation_time));
            fmt::print("==============================\n\n");
        }

        fmt::print("Adjust ");
        fmt::print(fmt::fg(fmt::terminal_color::green), "{}\n", key);
        auto closest_edit_distance = std::size_t(-1);
        bool value_influenced_output = false;
        std::string improvement_value;
        auto evaluation_start = std::chrono::steady_clock::now();
        if (possible_values.options.size() > 1) {
            std::vector<futures::cfuture<std::size_t>> evaluation_tasks;
            // Launch evaluation tasks
            for (std::size_t i = 0; i < possible_values.options.size(); ++i) {
                const auto &possible_value = *(possible_values.options.begin() + i);
                evaluation_tasks.emplace_back(futures::async(ex, [=, key = key]() mutable {
                    // Copy experiment files
                    fs::path task_temp = config.temp / fmt::format("temp_{}", i);
                    fs::copy(config.input, task_temp,
                             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                    current_cf.emplace_back(clang_format_entry{key, possible_value, true, 0});
                    save(current_cf, task_temp / ".clang-format");

                    // Evaluate
                    std::size_t dist = evaluate(config, task_temp);
                    return dist;
                }));
            }

            // Get and analyse results for parameter
            for (std::size_t i = 0; i < possible_values.options.size(); ++i) {
                const auto &possible_value = possible_values.options[i];

                // Message
                fmt::print("Evaluating {}: ", key);
                fmt::print(fmt::fg(fmt::terminal_color::blue), "{}", possible_value);
                std::cout << std::flush;

                // Print some info
                if (std::size_t dist = evaluation_tasks[i].get(); dist == std::size_t(-1)) {
                    fmt::print(fmt::fg(fmt::terminal_color::yellow), " (failed option)\n");
                } else {
                    const bool improved = dist < closest_edit_distance;
                    const bool closest_is_concrete_value = closest_edit_distance != std::size_t(-1);
                    if (!value_influenced_output) {
                        value_influenced_output = dist != closest_edit_distance && closest_is_concrete_value;
                    }
                    if (improved && closest_is_concrete_value) {
                        fmt::print(fmt::fg(fmt::terminal_color::green), " (improved edit distance to {})\n", dist);
                    } else {
                        fmt::print(" (edit distance {})\n", dist);
                    }
                    if (improved) {
                        closest_edit_distance = dist;
                        improvement_value = possible_value;
                    }
                }
                ++total_neighbors_evaluated;
            }

            // Update the main file
            if (!improvement_value.empty() && value_influenced_output) {
                current_cf.emplace_back(clang_format_entry{key, improvement_value, value_influenced_output,
                                                           closest_edit_distance,
                                                           closest_edit_distance == std::size_t(-1)});
            } else if (!value_influenced_output) {
                if (!config.require_influence) {
                    if (improvement_value.empty()) {
                        improvement_value = possible_values.options.front();
                    }
                    current_cf.emplace_back(clang_format_entry{key, improvement_value, value_influenced_output,
                                                               closest_edit_distance,
                                                               closest_edit_distance == std::size_t(-1)});
                    if (closest_edit_distance == std::size_t(-1)) {
                        current_cf.back().failed = true;
                    }
                }
                fmt::print(fmt::fg(fmt::terminal_color::yellow), "Parameter {} did not affect the output\n", key);
            }
        } else {
            fmt::print(fmt::fg(fmt::terminal_color::green), "Single option for {}: {}\n", key,
                       possible_values.options.front());
            current_cf.emplace_back(
                clang_format_entry{key, possible_values.options.front(), true, 0, false, "single option"});
            ++total_neighbors_evaluated;
        }
        if (!current_cf.empty()) {
            save(current_cf, config.output);
        }
        fmt::print("\n");

        // Undo requirements, unless the new score is already better anyway
        if (req_applied && prev_entry.score < closest_edit_distance) {
            auto it = std::find_if(current_cf.begin(), current_cf.end(),
                                   [&](clang_format_entry &e) { return e.key == prev_entry.key; });
            if (it != current_cf.end()) {
                *it = prev_entry;
            }
        }

        // Update time estimate
        auto evaluation_end = std::chrono::steady_clock::now();
        auto evaluation_time = evaluation_end - evaluation_start;
        total_evaluation_time += evaluation_time;
    }

    // Fix the ones that failed or did not affect the output
    // Fix with common prefix
    for (clang_format_entry &entry : current_cf) {
        if (entry.failed || !entry.affected_output) {
            if (entry.failed) {
                fmt::print(fmt::fg(fmt::terminal_color::yellow), "Inheriting failed entry {}\n", entry.key);
            } else {
                fmt::print(fmt::fg(fmt::terminal_color::yellow), "Inheriting innocuous entry {}\n", entry.key);
            }
            auto opts_it = std::find_if(cf_opts.begin(), cf_opts.end(), [&](auto &p) { return p.first == entry.key; });
            auto &opts = opts_it->second;
            // Try to inherit from other similar prefixes
            if (!opts.default_value_from_prefix.empty()) {
                std::vector<std::pair<std::string, std::size_t>> other_entries_count;
                for (auto &other_entry : current_cf) {
                    bool inherit_value = !other_entry.failed && other_entry.affected_output &&
                                         starts_with(other_entry.key, opts.default_value_from_prefix);
                    if (inherit_value) {
                        auto it = std::find_if(other_entries_count.begin(), other_entries_count.end(),
                                               [&](std::pair<std::string, std::size_t> const &oe) {
                                                   return oe.first == other_entry.value;
                                               });
                        if (it != other_entries_count.end()) {
                            ++it->second;
                        } else {
                            other_entries_count.emplace_back(other_entry.value, 1);
                        }
                    }
                }
                std::sort(other_entries_count.begin(), other_entries_count.end(),
                          [](auto &a, auto &b) { return a.second > b.second; });
                for (auto const &[value, count] : other_entries_count) {
                    auto it = std::find(opts.options.begin(), opts.options.end(), value);
                    if (bool option_is_valid = it != opts.options.end(); option_is_valid) {
                        entry.value = value;
                        entry.affected_output = true;
                        entry.failed = false;
                        entry.comment = fmt::format("inherited from prefix {}", opts.default_value_from_prefix);
                        fmt::print(fmt::fg(fmt::terminal_color::green), "    Inheriting value {} from prefix {} for {}\n",
                                   value, opts.default_value_from_prefix, entry.key);
                        break;
                    }
                }
            }
        }
    }

    // Fix with default values
    for (clang_format_entry &entry : current_cf) {
        if (entry.failed || !entry.affected_output) {
            if (entry.failed) {
                fmt::print(fmt::fg(fmt::terminal_color::yellow), "Finding default for failed entry {}\n", entry.key);
            } else {
                fmt::print(fmt::fg(fmt::terminal_color::yellow), "Finding default for innocuous entry {}\n", entry.key);
            }
            auto opts_it = std::find_if(cf_opts.begin(), cf_opts.end(), [&](auto &p) { return p.first == entry.key; });
            auto &opts = opts_it->second;
            if (!opts.default_value.empty()) {
                entry.value = opts.default_value;
                entry.affected_output = true;
                entry.failed = false;
                entry.comment = fmt::format("Using default value {}", opts.default_value);
                fmt::print(fmt::fg(fmt::terminal_color::green), "    Using default value {} for {}\n", opts.default_value,
                           entry.key);
            }
        }
    }

    save(current_cf, config.output);
}

int main(int argc, char **argv) {
    auto [desc, config] = parse_cli(argc, argv);
    if (config.help || !validate_input(config)) {
        print_help(desc);
        return 1;
    }
    clang_format_local_search(config);
    return 0;
}
