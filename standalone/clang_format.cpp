//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#include "clang_format.hpp"
#include <fmt/color.h>
#include <fmt/format.h>
#include <algorithm>
#include <fstream>

std::vector<std::pair<std::string, clang_format_possible_values>>
generate_clang_format_options() {
    auto starts_with = [](std::string_view str, std::string_view substr) {
        return str.substr(0, substr.size()) == substr;
    };

    auto ends_with = [](std::string_view str, std::string_view substr) {
        return str.size() > substr.size()
               && str.substr(str.size() - substr.size(), substr.size())
                      == substr;
    };

    // list of all values
    std::vector<std::pair<std::string, clang_format_possible_values>> result{
  // Language, this format style is targeted at.
        {                                             "Language",{ "Cpp" }                                                                 },
 // The style used for all options not specifically set
        {                                         "BasedOnStyle",
         { "LLVM",
         "Google",
         "Chromium",
         "Mozilla",
         "WebKit",
         "Microsoft",
         "GNU" }                                                                                                },
 // The extra indent or outdent of access modifiers
        {                                 "AccessModifierOffset",
         { "-8",
         "-6",
         "-4",
         "-2",
         "0",
         "2",
         "4",
         "6",
         "8" }                                                                                                  }, // horizontally aligns arguments after an open bracket
        {                                "AlignAfterOpenBracket",
         { "BAS_Align", "BAS_DontAlign", "BAS_AlwaysBreak" }                                                    },
 // when using initialization for an array of structs aligns the fields
  // into columns
        {                               "AlignArrayOfStructures",
         { "AIAS_Left", "AIAS_Right", "AIAS_None" }                                                             },
 // Style of aligning consecutive assignments
        {                          "AlignConsecutiveAssignments",
         { "ACS_None",
         "ACS_Consecutive",
         "ACS_AcrossEmptyLines",
         "ACS_AcrossComments",
         "ACS_AcrossEmptyLinesAndComments" }                                                                    },
 // Style of aligning consecutive bit field
        {                            "AlignConsecutiveBitFields",
         { "ACS_None",
         "ACS_Consecutive",
         "ACS_AcrossEmptyLines",
         "ACS_AcrossComments",
         "ACS_AcrossEmptyLinesAndComments" }                                                                    },
 // Style of aligning consecutive declarations
        {                         "AlignConsecutiveDeclarations",
         { "ACS_None",
         "ACS_Consecutive",
         "ACS_AcrossEmptyLines",
         "ACS_AcrossComments",
         "ACS_AcrossEmptyLinesAndComments" }                                                                    },
 // Style of aligning consecutive declarations
        {                               "AlignConsecutiveMacros",
         { "ACS_None",
         "ACS_Consecutive",
         "ACS_AcrossEmptyLines",
         "ACS_AcrossComments",
         "ACS_AcrossEmptyLinesAndComments" }                                                                    },
 // Options for aligning backslashes in escaped newlines
        {                                 "AlignEscapedNewlines",
         { "ENAS_DontAlign", "ENAS_Left", "ENAS_Right" }                                                        },
 // horizontally align operands of binary and ternary expressions
        {                                        "AlignOperands",
         { "OAS_DontAlign", "OAS_Align", "OAS_AlignAfterOperator" }                                             },
 // aligns trailing comments
        {                                "AlignTrailingComments",                            { "true", "false" }},
 // allow putting all arguments onto the next line
        {                          "AllowAllArgumentsOnNextLine",                            { "true", "false" }},
 // putting all parameters of a function declaration onto the next line
        {            "AllowAllParametersOfDeclarationOnNextLine",                            { "true", "false" }},
 // "while (true) { continue }" can be put on a single line
        {                        "AllowShortBlocksOnASingleLine",
         { "SBS_Never", "SBS_Empty", "SBS_Always" }                                                             },
 // short case labels will be contracted to a single line.
        {                    "AllowShortCaseLabelsOnASingleLine",                            { "true", "false" }},
 // Allow short enums on a single line.
        {                         "AllowShortEnumsOnASingleLine",                            { "true", "false" }},
 // int f() { return 0; } can be put on a single line
        {                     "AllowShortFunctionsOnASingleLine",
         { "SFS_None",
         "SFS_InlineOnly",
         "SFS_Empty",
         "SFS_Inline",
         "SFS_All" }                                                                                            },
 // if (a) return can be put on a single line.
        {                  "AllowShortIfStatementsOnASingleLine",
         { "SIS_Never",
         "SIS_WithoutElse",
         "SIS_OnlyFirstIf",
         "SIS_AllIfsAndElse" }                                                                                  },
 // auto lambda []() { return 0; } can be put on a single line.
        {                       "AllowShortLambdasOnASingleLine",
         { "SLS_None", "SLS_Empty", "SLS_Inline", "SLS_All" }                                                   },
 // If true, while (true) continue can be put on a single line.
        {                         "AllowShortLoopsOnASingleLine",                            { "true", "false" }},
 // The function definition return type breaking style to use.
        {                 "AlwaysBreakAfterDefinitionReturnType",
         { "DRTBS_None", "DRTBS_All", "DRTBS_TopLevel" }                                                        },
 // The function declaration return type breaking style to use.
        {                           "AlwaysBreakAfterReturnType",
         { "RTBS_None",
         "RTBS_All",
         "RTBS_TopLevel",
         "RTBS_AllDefinitions",
         "RTBS_TopLevelDefinitions" }                                                                           },
 // If true, always break before multiline string literals.
        {                    "AlwaysBreakBeforeMultilineStrings",                            { "true", "false" }},
 // The template declaration breaking style to use.
        {                      "AlwaysBreakTemplateDeclarations",
         { "BTDS_No", "BTDS_MultiLine", "BTDS_Yes" }                                                            },
 // If false, a function call’s arguments will either be all on the same
  // line or will have one line each.
        {                                     "BinPackArguments",                            { "true", "false" }},
 // If false, a function declaration’s or function definition’s
  // parameters will either all be on the same
  // line or
  // will have one line each.
        {                                    "BinPackParameters",                            { "true", "false" }},
 // The BitFieldColonSpacingStyle to use for bitfields.
        {                                 "BitFieldColonSpacing",
         { "BFCS_Both", "BFCS_None", "BFCS_Before", "BFCS_After" }                                              },
 // The brace breaking style to use.
        {                                    "BreakBeforeBraces",
         { "BS_Attach",
         "BS_Linux",
         "BS_Mozilla",
         "BS_Stroustrup",
         "BS_Allman",
         "BS_Whitesmiths",
         "BS_GNU",
         "BS_WebKit",
         "BS_Custom" }                                                                                          },
 // Control of individual brace wrapping cases.
        {                         "BraceWrapping.AfterCaseLabel",                            { "true", "false" }},
        {                             "BraceWrapping.AfterClass",                            { "true", "false" }},
        {                  "BraceWrapping.AfterControlStatement",
         { "BWACS_Never", "BWACS_MultiLine", "BWACS_Always" }                                                   },
        {                              "BraceWrapping.AfterEnum",                            { "true", "false" }},
        {                          "BraceWrapping.AfterFunction",                            { "true", "false" }},
        {                         "BraceWrapping.AfterNamespace",                            { "true", "false" }},
        {                   "BraceWrapping.AfterObjCDeclaration",                            { "true", "false" }},
        {                            "BraceWrapping.AfterStruct",                            { "true", "false" }},
        {                             "BraceWrapping.AfterUnion",                            { "true", "false" }},
        {                       "BraceWrapping.AfterExternBlock",                            { "true", "false" }},
        {                            "BraceWrapping.BeforeCatch",                            { "true", "false" }},
        {                             "BraceWrapping.BeforeElse",                            { "true", "false" }},
        {                       "BraceWrapping.BeforeLambdaBody",                            { "true", "false" }},
        {                            "BraceWrapping.BeforeWhile",                            { "true", "false" }},
        {                           "BraceWrapping.IndentBraces",                            { "true", "false" }},
        {                     "BraceWrapping.SplitEmptyFunction",                            { "true", "false" }},
        {                       "BraceWrapping.SplitEmptyRecord",                            { "true", "false" }},
        {                    "BraceWrapping.SplitEmptyNamespace",                            { "true", "false" }},
 // Break after each annotation on a field in Java files.
        {                       "BreakAfterJavaFieldAnnotations",                            { "true", "false" }},
 // The way to wrap binary operators.
        {                           "BreakBeforeBinaryOperators",
         { "BOS_None", "BOS_NonAssignment", "BOS_All" }                                                         },
 // If true, concept will be placed on a new line.
        {                       "BreakBeforeConceptDeclarations",                            { "true", "false" }},
 // If true, ternary operators will be placed after line breaks.
        {                          "BreakBeforeTernaryOperators",                            { "true", "false" }},
 // The break constructor initializers style to use.
        {                         "BreakConstructorInitializers",
         { "BCIS_BeforeColon", "BCIS_BeforeComma", "BCIS_AfterColon" }                                          },
 // The inheritance list style to use.
        {                                 "BreakInheritanceList",
         { "BILS_BeforeColon",
         "BILS_BeforeComma",
         "BILS_AfterColon",
         "BILS_AfterComma" }                                                                                    },
 // Allow breaking string literals when formatting.
        {                                  "BreakStringLiterals",                            { "true", "false" }},
 // The column limit
        {                                          "ColumnLimit",      { "40", "60", "80", "100", "120", "140" }},
 // If true, consecutive namespace declarations will be on the same line.
  // If false, each namespace is
  // declared on
  // a new line.
        {                                    "CompactNamespaces",                            { "true", "false" }},
 // The number of characters to use for indentation of constructor
  // initializer lists as well as inheritance
  // lists.
        {                    "ConstructorInitializerIndentWidth",
         { "0", "2", "4", "6", "8", "10", "12" }                                                                },
 // Indent width for line continuations.
        {                              "ContinuationIndentWidth",              { "0", "2", "4", "6", "8", "10" }},
 // If true, format braced lists as best suited for C++11 braced lists..
        {                                 "Cpp11BracedListStyle",                            { "true", "false" }},
 // Analyze the formatted file for the most used line ending (\r\n or
  // \n). UseCRLF is only used as a fallback
  // if
  // none can be derived.
        {                                     "DeriveLineEnding",                            { "true", "false" }},
 // If true, analyze the formatted file for the most common alignment of
  // & and *.
        {                               "DerivePointerAlignment",                            { "true", "false" }},
 // Defines when to put an empty line after access modifiers.
        {                         "EmptyLineAfterAccessModifier",
         { "ELAAMS_Never", "ELAAMS_Leave", "ELAAMS_Always" }                                                    },
 // Defines in which cases to put empty line before access modifiers.
        {                        "EmptyLineBeforeAccessModifier",
         { "ELBAMS_Never",
         "ELBAMS_Leave",
         "ELBAMS_LogicalBlock",
         "ELBAMS_Always" }                                                                                      },
 // If true, clang-format detects whether function calls and definitions
  // are formatted with one parameter per
  // line.
        {                     "ExperimentalAutoDetectBinPacking",                            { "true", "false" }},
 // If true, clang-format adds missing namespace end comments for short
  // namespaces and fixes invalid existing
  // ones..
        {                                 "FixNamespaceComments",                            { "true", "false" }},
 // Dependent on the value, multiple #include blocks can be sorted as one
  // and divided based on category.
        {                                        "IncludeBlocks", { "IBS_Preserve", "IBS_Merge", "IBS_Regroup" }},
 // Specify whether access modifiers should have their own indentation
  // level.
        {                                "IndentAccessModifiers",                            { "true", "false" }},
 // Indent case label blocks one level from the case label.
        {                                     "IndentCaseBlocks",                            { "true", "false" }},
 // Indent case labels one level from the switch statement.
        {                                     "IndentCaseLabels",                            { "true", "false" }},
 // IndentExternBlockStyle is the type of indenting of extern blocks..
        {                                    "IndentExternBlock",
         { "IEBS_AfterExternBlock", "IEBS_NoIndent", "IEBS_Indent" }                                            },
 // Indent goto labels.
        {                                     "IndentGotoLabels",                            { "true", "false" }},
 // The preprocessor directive indenting style to use.
        {                                   "IndentPPDirectives",
         { "PPDIS_None", "PPDIS_AfterHash", "PPDIS_BeforeHash" }                                                },
 // Indent the requires clause in a template.
        {                                       "IndentRequires",                            { "true", "false" }},
 // The number of columns to use for indentation..
        {                                          "IndentWidth",                    { "0", "2", "4", "6", "8" }},
 // Indent if a function definition or declaration is wrapped after the
  // type.
        {                           "IndentWrappedFunctionNames",                            { "true", "false" }},
 // Indent if a function definition or declaration is wrapped after the
  // type.
        {                                 "InsertTrailingCommas",                  { "TCS_None", "TCS_Wrapped" }},
 // If true, the empty line at the start of blocks is kept.
        {                     "KeepEmptyLinesAtTheStartOfBlocks",                            { "true", "false" }},
 // The indentation style of lambda bodies.
        {                                "LambdaBodyIndentation",          { "LBI_Signature", "LBI_OuterScope" }},
 // The maximum number of consecutive empty lines to keep.
        {                                  "MaxEmptyLinesToKeep",                   { "0", "2", "4", "8", "16" }},
 // The indentation used for namespaces.
        {                                 "NamespaceIndentation",            { "NI_None", "NI_Inner", "NI_All" }},
 // The pack constructor initializers style to use.
        {                          "PackConstructorInitializers",
         { "PCIS_Never",
         "PCIS_BinPack",
         "PCIS_CurrentLine",
         "PCIS_NextLine" }                                                                                      },
 // The penalty for breaking around an assignment operator.
        {                               "PenaltyBreakAssignment",
         { "2",
         "4",
         "8",
         "16",
         "32",
         "64",
         "128",
         "256",
         "512",
         "1024",
         "2048" }                                                                                               },
 // The penalty for breaking a function call after call
        {                 "PenaltyBreakBeforeFirstCallParameter",
         { "2",
         "4",
         "8",
         "16",
         "32",
         "64",
         "128",
         "256",
         "512",
         "1024",
         "2048" }                                                                                               },
 // The penalty for each line break introduced inside a comment.
        {                                  "PenaltyBreakComment",
         { "2",
         "4",
         "8",
         "16",
         "32",
         "64",
         "128",
         "256",
         "512",
         "1024",
         "2048" }                                                                                               },
 // The penalty for breaking before the first <<.
        {                            "PenaltyBreakFirstLessLess",
         { "2",
         "4",
         "8",
         "16",
         "32",
         "64",
         "128",
         "256",
         "512",
         "1024",
         "2048" }                                                                                               },
 // The penalty for breaking after (.
        {                          "PenaltyBreakOpenParenthesis",
         { "2",
         "4",
         "8",
         "16",
         "32",
         "64",
         "128",
         "256",
         "512",
         "1024",
         "2048" }                                                                                               },
 // The penalty for each line break introduced inside a string literal.
        {                                   "PenaltyBreakString",
         { "2",
         "4",
         "8",
         "16",
         "32",
         "64",
         "128",
         "256",
         "512",
         "1024",
         "2048" }                                                                                               },
 // The penalty for breaking after template declaration.
        {                      "PenaltyBreakTemplateDeclaration",
         { "2",
         "4",
         "8",
         "16",
         "32",
         "64",
         "128",
         "256",
         "512",
         "1024",
         "2048" }                                                                                               },
 // The penalty for each character outside of the column limit.
        {                               "PenaltyExcessCharacter",
         { "2",
         "4",
         "8",
         "16",
         "32",
         "64",
         "128",
         "256",
         "512",
         "1024",
         "2048" }                                                                                               },
 // Penalty for each character of whitespace indentation (counted
  // relative to leading non-whitespace column).
        {                            "PenaltyIndentedWhitespace",
         { "2",
         "4",
         "8",
         "16",
         "32",
         "64",
         "128",
         "256",
         "512",
         "1024",
         "2048" }                                                                                               },
 // Penalty for putting the return type of a function onto its own line.
        {                        "PenaltyReturnTypeOnItsOwnLine",
         { "2",
         "4",
         "8",
         "16",
         "32",
         "64",
         "128",
         "256",
         "512",
         "1024",
         "2048" }                                                                                               },
 // Pointer and reference alignment style.
        {                                     "PointerAlignment",      { "PAS_Left", "PAS_Right", "PAS_Middle" }},
 // Different ways to arrange specifiers and qualifiers (e.g.
  // const/volatile)
        {                                   "QualifierAlignment",
         { "QAS_Leave", "QAS_Left", "QAS_Right", "QAS_Custom" }                                                 },
 // Reference alignment style (overrides PointerAlignment for  references).
        {                                   "ReferenceAlignment",
         { "RAS_Pointer", "RAS_Left", "RAS_Right", "RAS_Middle" }                                               },
 // Remove optional braces of control statements
        {                                     "RemoveBracesLLVM",                            { "true", "false" }},
 // If true, clang-format will attempt to re-flow comments.
        {                                       "ReflowComments",                            { "true", "false" }},
 // The position of the requires clause.
        {                               "RequiresClausePosition",
         { "RCPS_OwnLine",
         "RCPS_WithPreceding",
         "RCPS_WithFollowing",
         "RCPS_SingleLine" }                                                                                    },
 // Specifies the use of empty lines to separate definition blocks
        {                             "SeparateDefinitionBlocks",
         { "SDS_Leave", "SDS_Always", "SDS_Never" }                                                             },
 // The maximal number of unwrapped lines that a short namespace spans.
        {                                  "ShortNamespaceLines",                         { "0", "1", "4", "8" }},
 // Controls if and how clang-format will sort #includes.
        {                                         "SortIncludes",
         { "SI_Never", "SI_CaseSensitive", "SI_CaseInsensitive" }                                               },
 // If true, clang-format will sort using declarations.
        {                                "SortUsingDeclarations",                            { "true", "false" }},
 // If true, a space is inserted after C style casts.
        {                                 "SpaceAfterCStyleCast",                            { "true", "false" }},
 // If true, a space is inserted after the logical not operator (!).
        {                                 "SpaceAfterLogicalNot",                            { "true", "false" }},
 // If true, a space will be inserted after the ‘template’ keyword.
        {                            "SpaceAfterTemplateKeyword",                            { "true", "false" }},
 // Defines in which cases to put a space before or after pointer
  // qualifiers
        {                         "SpaceAroundPointerQualifiers",
         { "SAPQ_Default", "SAPQ_Before", "SAPQ_After", "SAPQ_Both" }                                           },
 // If false, spaces will be removed before assignment operators.
        {                       "SpaceBeforeAssignmentOperators",                            { "true", "false" }},
 // If false, spaces will be removed before case colon.
        {                                 "SpaceBeforeCaseColon",                            { "true", "false" }},
 // If true, a space will be inserted before a C++11 braced list used to
  // initialize an object (after the
  // preceding identifier or type).
        {                           "SpaceBeforeCpp11BracedList",                            { "true", "false" }},
 // If false, spaces will be removed before constructor initializer  colon.
        {                      "SpaceBeforeCtorInitializerColon",                            { "true", "false" }},
 // If false, spaces will be removed before inheritance colon.
        {                          "SpaceBeforeInheritanceColon",                            { "true", "false" }},
 // Defines in which cases to put a space before opening parentheses.
        {                                    "SpaceBeforeParens",
         { "SBPO_Never",
         "SBPO_ControlStatements",
         "SBPO_ControlStatementsExceptControlMacros",
         "SBPO_NonEmptyParentheses",
         "SBPO_Always",
         "SBPO_Custom" }                                                                                        },
 // Put space betwee control statement keywords
        {      "SpaceBeforeParensOptions.AfterControlStatements",
         { "true", "false" }                                                                                    },
 // space between foreach macros and opening parentheses
        {          "SpaceBeforeParensOptions.AfterForeachMacros",                            { "true", "false" }},
 // space between function declaration name and opening parentheses
        {"SpaceBeforeParensOptions.AfterFunctionDeclarationName",
         { "true", "false" }                                                                                    },
 // space between function declaration name and opening parentheses
        {"SpaceBeforeParensOptions.AfterFunctionDeclarationName",
         { "true", "false" }                                                                                    },
 // between function definition name and opening parentheses
        { "SpaceBeforeParensOptions.AfterFunctionDefinitionName",
         { "true", "false" }                                                                                    },
 // space between if macros and opening parentheses
        {               "SpaceBeforeParensOptions.AfterIfMacros",                            { "true", "false" }},
 // space between operator overloading and opening parentheses
        {     "SpaceBeforeParensOptions.AfterOverloadedOperator",
         { "true", "false" }                                                                                    },
 // put space between requires keyword in a requires clause and opening
  // parentheses
        {       "SpaceBeforeParensOptions.AfterRequiresInClause",
         { "true", "false" }                                                                                    },
 // space between requires keyword in a requires expression and opening
  // parentheses
        {   "SpaceBeforeParensOptions.AfterRequiresInExpression",
         { "true", "false" }                                                                                    },
 // space before opening parentheses only if the parentheses are not  empty
        {   "SpaceBeforeParensOptions.BeforeNonEmptyParentheses",
         { "true", "false" }                                                                                    },
 // If false, spaces will be removed before range-based for loop colon.
        {                    "SpaceBeforeRangeBasedForLoopColon",                            { "true", "false" }},
 // If true, spaces will be before [. Lambdas will not be affected. Only
  // the first [ will get a space added.
        {                            "SpaceBeforeSquareBrackets",                            { "true", "false" }},
 // If true, spaces will be inserted into {}.
        {                                    "SpaceInEmptyBlock",                            { "true", "false" }},
 // If true, spaces may be inserted into ().
        {                              "SpaceInEmptyParentheses",                            { "true", "false" }},
 // The number of spaces before trailing line comments (// - comments).
        {                         "SpacesBeforeTrailingComments",                    { "0", "1", "2", "4", "8" }},
 // The SpacesInAnglesStyle to use for template argument lists.
        {                                       "SpacesInAngles",  { "SIAS_Never", "SIAS_Always", "SIAS_Leave" }},
 // If true, spaces may be inserted into C style casts.
        {                        "SpacesInCStyleCastParentheses",                            { "true", "false" }},
 // If true, spaces will be inserted around if/for/switch/while
  // conditions.
        {                         "SpacesInConditionalStatement",                            { "true", "false" }},
 // If true, spaces are inserted inside container literals (e.g. ObjC and
  // Javascript array and dict
  // literals).
        {                            "SpacesInContainerLiterals",                            { "true", "false" }},
 // If true, spaces will be inserted after ( and before ).
        {                                  "SpacesInParentheses",                            { "true", "false" }},
 // Parse and format C++ constructs compatible with this standard.
        {                                             "Standard",
         { "c++03", "c++11", "c++14", "c++17", "c++20", "Latest", "Auto" }                                      },
 // The number of columns used for tab stops.
        {                                             "TabWidth",                    { "0", "2", "4", "6", "8" }},
 // Use \r\n instead of \n for line breaks. Also used as fallback if
  // DeriveLineEnding is true.
        {                                              "UseCRLF",                            { "true", "false" }},
 // The way to use tab characters in the resulting file.
        {                                               "UseTab",
         { "UT_Never",
         "UT_ForIndentation",
         "UT_ForContinuationAndIndentation",
         "UT_AlignWithSpaces",
         "UT_Always" }                                                                                          },
    };

    // # Set requirements
    // ## All BraceWrapping suboptions only make sense when BreakBeforeBraces is
    // Custom
    for (auto &[key, value]: result) {
        if (starts_with(key, "BraceWrapping.")) {
            value.requirements = { "BreakBeforeBraces", "BS_Custom" };
        }
    }

    // ## All SpaceBeforeParensOptions suboptions only make sense when
    // SpaceBeforeParens is SBPO_Custom
    for (auto &[key, value]: result) {
        if (starts_with(key, "SpaceBeforeParensOptions.")) {
            value.requirements = { "SpaceBeforeParens", "SBPO_Custom" };
        }
    }

    // set default_value_from_prefix
    // Variables with these prefixes might inherit default values from other
    // options with the same prefix
    for (auto &[key, value]: result) {
        for (auto &prefix:
             { "Align",
               "AlwaysBreak",
               "BraceWrapping",
               "BreakBefore",
               "Derive",
               "EmptyLine",
               "Indent",
               "PenaltyBreak",
               "SpaceAfter",
               "SpaceBefore",
               "SpaceIn" })
        {
            if (starts_with(key, prefix)) {
                value.default_value_from_prefix = prefix;
            }
        }
    }

    // set default_value
    // We set the default values according to what is reasonable for each option
    // prefix
    for (auto &[key, value]: result) {
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
        for (auto &[prefix, reasonable_defaults]: prefix_defaults) {
            if (starts_with(key, prefix)) {
                auto it = std::find_if(
                    reasonable_defaults.begin(),
                    reasonable_defaults.end(),
                    [value = value, &ends_with](std::string const &def) {
                    return std::find_if(
                               value.options.begin(),
                               value.options.end(),
                               [&](const std::string &opt) {
                        return ends_with(opt, def);
                               })
                        != value.options.end();
                    });
                if (it != reasonable_defaults.end()) {
                    value.default_value = *it;
                }
            }
        }
    }

    return result;
}

void
print(const std::vector<clang_format_entry> &current_cf) {
    std::string prev_section;
    for (const auto &[key, value, affected_output, score, failed, comment]:
         current_cf)
    {
        // Key
        std::size_t key_width = 0;
        auto subsection_begin = key.find_first_of('.');
        if (bool section_only = subsection_begin == std::string::npos;
            section_only)
        {
            if (failed) {
                fmt::print("# ");
            }
            fmt::print(fmt::fg(fmt::terminal_color::green), "{}", key);
            key_width = key.size();
        } else {
            auto section = std::string_view(key).substr(0, subsection_begin);
            auto subsection = std::string_view(key).substr(
                subsection_begin + 1);
            if (prev_section != section) {
                fmt::print(
                    fmt::fg(fmt::terminal_color::green),
                    "{}:\n",
                    section);
                prev_section = section;
            }
            if (failed) {
                fmt::print("# ");
            }
            fmt::print(fmt::fg(fmt::terminal_color::green), "  {}", subsection);
            key_width = subsection.size() + 2;
        }
        fmt::print(": ");

        // Value
        std::string_view value_view = value;
        if (auto last_under = value_view.find_last_of('_');
            last_under != std::string_view::npos)
        {
            value_view = value_view.substr(last_under + 1);
        }
        fmt::print(fmt::fg(fmt::terminal_color::blue), "{}", value_view);
        std::size_t value_width = value_view.size();
        std::size_t entry_width = key_width + value_width + 2;
        std::size_t comment_padding
            = entry_width > 50 ?
                  0 :
                  50 - entry_width - 2 * static_cast<int>(failed);
        const std::string empty_str;
        if (!comment.empty()) {
            fmt::print("{1: ^{2}} # {0}", comment, empty_str, comment_padding);
        } else if (failed) {
            fmt::print(
                "{0: ^{1}} # parameter not available",
                empty_str,
                comment_padding);
        } else if (!affected_output) {
            fmt::print(
                "{0: ^{1}} # did not affect the output",
                empty_str,
                comment_padding);
        } else if (score != 0) {
            fmt::print(
                "{1: ^{2}} # edit distance {0}",
                score,
                empty_str,
                comment_padding);
        }
        fmt::print("\n");
    }
}

void
save(
    const std::vector<clang_format_entry> &current_cf,
    const std::filesystem::path &output) {
    std::ofstream fout(output);
    fout << "# .clang-format inferred with clang-unformat "
            "(https://www.github.com/alandefreitas/clang-unformat)\n";
    std::string prev_section;
    std::size_t key_width = 0;
    for (const auto &[key, value, affected_output, score, failed, comment]:
         current_cf)
    {
        auto subsection_begin = key.find_first_of('.');
        if (bool section_only = subsection_begin == std::string::npos;
            section_only)
        {
            if (failed) {
                fout << fmt::format("# ");
            }
            fout << fmt::format("{}", key);
            key_width = key.size();
        } else {
            auto section = std::string_view(key).substr(0, subsection_begin);
            auto subsection = std::string_view(key).substr(
                subsection_begin + 1);
            if (prev_section != section) {
                fout << fmt::format("{}:\n", section);
                prev_section = section;
            }
            if (failed) {
                fout << fmt::format("# ");
            }
            fout << fmt::format("  {}", subsection);
            key_width = subsection.size() + 2;
        }
        std::string_view value_view = value;
        auto last_under = value_view.find_last_of('_');
        if (bool needs_split = last_under != std::string_view::npos;
            needs_split)
        {
            value_view = value_view.substr(last_under + 1);
        }
        fout << fmt::format(": {}", value_view);
        std::size_t value_width = value_view.size();
        std::size_t entry_width = key_width + value_width + 2;
        std::size_t comment_padding
            = entry_width > 50 ? 0 : 50 - entry_width - (failed ? 2 : 0);
        const std::string empty_str;
        if (!comment.empty()) {
            fout << fmt::format(
                "{1: ^{2}} # {0}",
                comment,
                empty_str,
                comment_padding);
        } else if (failed) {
            fout << fmt::format(
                "{0: ^{1}} # parameter not available",
                empty_str,
                comment_padding);
        } else if (!affected_output) {
            fout << fmt::format(
                "{0: ^{1}} # did not affect the output",
                empty_str,
                comment_padding);
        } else if (score != 0) {
            fout << fmt::format(
                "{1: ^{2}} # edit distance {0}",
                score,
                empty_str,
                comment_padding);
        }
        fout << fmt::format("\n");
    }
}
