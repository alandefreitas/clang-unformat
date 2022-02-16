cmake_minimum_required(VERSION 3.21)
project(clang_unformat)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads)
find_package(Boost 1.78.0 COMPONENTS process program_options filesystem)
include(FetchContent)
find_package(fmt QUIET)
if (NOT fmt_FOUND)
    FetchContent_Declare(fmt URL https://github.com/fmtlib/fmt/releases/download/8.1.1/fmt-8.1.1.zip)
    FetchContent_MakeAvailable(fmt)
endif()

FetchContent_Declare(futures URL https://github.com/alandefreitas/futures/archive/refs/tags/v0.1.0.zip)
if(NOT futures_POPULATED)
    FetchContent_Populate(futures)
endif()

add_executable(clang-unformat standalone/main.cpp)

target_link_libraries(clang-unformat
    Boost::program_options
    Boost::filesystem
    Threads::Threads
    fmt::fmt
)
target_include_directories(clang-unformat PRIVATE ${futures_SOURCE_DIR}/source)
