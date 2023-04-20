#######################################################
### Clang-Unformat                                  ###
#######################################################
cmake_minimum_required(VERSION 3.10)
project(clang_unformat)

#######################################################
### Dependencies                                    ###
#######################################################
include(FetchContent)

# Threads
find_package(Threads)

# Boost
find_package(Boost 1.78.0 COMPONENTS process program_options asio)
if (NOT Boost_FOUND)
    set(BOOST_VERSION 1.81.0)
    message("Boost library not found or Version < 1.78.0 ; fetch and install Version ${BOOST_VERSION}")
    set(BOOST_INCLUDE_LIBRARIES process program_options asio)
    set(BOOST_ENABLE_CMAKE ON)

    FetchContent_Declare(
        Boost
        URL https://github.com/boostorg/boost/releases/download/boost-${BOOST_VERSION}/boost-${BOOST_VERSION}.zip
        GIT_TAG boost-${BOOST_VERSION}
    )
    FetchContent_MakeAvailable(Boost)
endif()

# fmt
find_package(fmt QUIET)
if (NOT fmt_FOUND)
    FetchContent_Declare(fmt URL https://github.com/fmtlib/fmt/releases/download/8.1.1/fmt-8.1.1.zip)
    FetchContent_MakeAvailable(fmt)
endif()

# futures
FetchContent_Declare(futures URL https://github.com/alandefreitas/futures/archive/refs/tags/v0.1.1.zip)
if(NOT futures_POPULATED)
    FetchContent_Populate(futures)
    add_library(futures_headers INTERFACE)
    target_include_directories(futures_headers INTERFACE ${futures_SOURCE_DIR}/source)
endif()

# edlib
find_package(edlib CONFIG QUIET)
if (NOT edlib_FOUND)
    FetchContent_Declare(edlib URL https://github.com/Martinsos/edlib/archive/refs/tags/v1.2.7.zip)
    if (NOT edlib_POPULATED)
        FetchContent_Populate(edlib)
        add_subdirectory(${edlib_SOURCE_DIR} EXCLUDE_FROM_ALL)
    endif()
endif()

#######################################################
### Executable                                      ###
#######################################################
add_executable(clang-unformat
        standalone/application.cpp
        standalone/application.hpp
        standalone/clang_format.cpp
        standalone/clang_format.hpp
        standalone/cli_config.cpp
        standalone/cli_config.hpp
        standalone/levenshtein.cpp
        standalone/levenshtein.hpp
        standalone/main.cpp)
target_include_directories(clang-unformat PRIVATE standalone)
target_compile_features(clang-unformat PRIVATE cxx_std_17)
target_link_libraries(clang-unformat
    Boost::program_options
    Boost::process
    Boost::asio
    Threads::Threads
    fmt::fmt
    futures_headers
    edlib::edlib
)
