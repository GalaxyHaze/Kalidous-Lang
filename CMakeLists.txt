cmake_minimum_required(VERSION 3.15)
project(Nova)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch CLI11
include(FetchContent)
FetchContent_Declare(
        cli11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG        v2.4.1  # ← Use latest stable version; check GitHub for updates
)
FetchContent_MakeAvailable(cli11)

FetchContent_Declare(
        unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG v4.0.0  # ou versão estável
)
FetchContent_MakeAvailable(unordered_dense)

add_definitions(-DNOVA_VERSION="${v.0.0-alpha}")

# Your source files
add_executable(Nova
        main.cpp
        include/Nova/memory/file.h
        include/Nova/parse/tokenizer.h
        include/Nova/core.h
        include/Nova/utils/helpers.h
        include/Nova/parse/tokens.h
        include/Nova/parse/tokens_map.h
        include/Nova/utils/debug.h
        include/Nova/parse/ast.h
        include/Nova/interface/CLI.h
        impl/memory/arena_c_functions.c
        impl/memory/file.c
        include/Nova/memory/bumpAllocator.h
        include/Nova/utils/i_o_utils.h
        impl/interface/CLI.cpp
        impl/parser/ast.cpp
)


# Link CLI11 (it's an interface library)
target_link_libraries(Nova PRIVATE CLI11::CLI11)
target_link_libraries(Nova PRIVATE unordered_dense::unordered_dense)