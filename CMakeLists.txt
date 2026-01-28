cmake_minimum_required(VERSION 3.15)
project(Nova LANGUAGES C CXX)

# --- Standards ---
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Output dirs (optional but clean) ---
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Dependencies ---
include(FetchContent)

FetchContent_Declare(
        cli11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.4.1
)
FetchContent_MakeAvailable(cli11)

FetchContent_Declare(
        unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG v4.0.0
)
FetchContent_MakeAvailable(unordered_dense)

# --- Version ---
add_definitions(-DNOVA_VERSION="v0.0-alpha")  # Note: removed ${} — it's not a CMake variable

set(LLVM_DIR /usr/lib/llvm21/lib/cmake/llvm)
find_package(LLVM REQUIRED CONFIG)

# --- Include directories (for all targets) ---
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# --- Core C Sources (Arena, File I/O, Utils) ---
set(CORE_C_SOURCES
        impl/memory/arena_c_functions.c
        impl/memory/file.c
        impl/utils/vector.c
)

# --- Parser Library (to be dynamically linked) ---
set (PARSER_CPP_SOURCES
        impl/parser/tokenizer.cpp
)

add_library(NovaParse SHARED ${PARSER_CPP_SOURCES}
)

target_link_libraries(NovaParse
        PRIVATE
        CLI11::CLI11
        unordered_dense::unordered_dense
)
target_include_directories(NovaParse
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# --- Main executable ---
add_executable(Nova main.cpp ${CORE_C_SOURCES})

target_link_libraries(Nova
        PRIVATE
        NovaParse
        CLI11::CLI11
        unordered_dense::unordered_dense
)
