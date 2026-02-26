cmake_minimum_required(VERSION 3.20)
project(Kalidous LANGUAGES C CXX)

# =============================================================================
# Standards & Policies
# =============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# =============================================================================
# Output Directories
# =============================================================================
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =============================================================================
# Fetch External Dependencies
# =============================================================================

include(FetchContent)

# --- CLI11 ---
FetchContent_Declare(cli11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG        v2.4.1
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(cli11)

# --- unordered_dense ---
FetchContent_Declare(unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG        v4.0.0
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(unordered_dense)

FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG        v3.4.0  # ou a versão mais recente
)

FetchContent_MakeAvailable(tomlplusplus)

# --- KalidousABI: Provides the 'Kalidous' INTERFACE target ---
FetchContent_Declare(KalidousABI
        GIT_REPOSITORY https://github.com/GalaxyHaze/Kalidous-Lang.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(KalidousABI)

#find_package(LLVM REQUIRED CONFIG)
#message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

# =============================================================================
# Compiler Flags
# =============================================================================
# Prefer target-specific flags over global add_definitions where possible
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    #add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        if(EXISTS "/usr/bin/ld.lld" OR EXISTS "/usr/local/bin/ld.lld")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
            message(STATUS "Using LLD linker")
        endif()
    endif()
endif()

# =============================================================================
# Sources
# =============================================================================
set(CORE_C_SOURCES
        impl/memory/arena_c_functions.c
        impl/memory/file.c
)

set(PARSER_CPP_SOURCES
        impl/parser/tokenizer.cpp
        impl/parser/keywords.cpp
        impl/ast/parser.cpp
        impl/ast/ast.h
)

# REMOVED parser.cpp from CLI_SOURCES to prevent duplicate compilation
set(CLI_SOURCES
        impl/interface/CLI.cpp
)

# =============================================================================
# Targets
# =============================================================================

# Core C library (implements parts of Kalidous ABI)
add_library(KalidousCore STATIC ${CORE_C_SOURCES})
target_include_directories(KalidousCore PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
# Link against Kalidous ABI → gets kalidous.h + C++17 requirement
target_link_libraries(KalidousCore PUBLIC Kalidous::Kalidous)

# Apply version definition specifically to targets that need it, or globally if preferred
target_compile_definitions(KalidousCore PRIVATE KALIDOUS_VERSION="v0.0-alpha")

target_link_libraries(KalidousCore PUBLIC tomlplusplus::tomlplusplus)

# Parser library
add_library(KalidousParse STATIC ${PARSER_CPP_SOURCES})
target_link_libraries(KalidousParse
        PUBLIC
        KalidousCore          # Brings in Kalidous::Kalidous transitively
        #LLVM       # Using modern CMake target for LLVM is safer
        unordered_dense::unordered_dense
)
target_compile_definitions(KalidousParse PRIVATE KALIDOUS_VERSION="v0.0-alpha")

set_target_properties(KalidousParse PROPERTIES
        VERSION 0.0.1
        SOVERSION 0
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

# Executable
add_executable(kalidous main.cpp ${CLI_SOURCES})
target_link_libraries(kalidous
        PRIVATE
        KalidousParse
        CLI11::CLI11
        # No need to link LLVM or unordered_dense here directly;
        # KalidousParse brings them in transitively if they are PUBLIC,
        # but since they are PRIVATE in KalidousParse, we only need KalidousParse logic.
)
target_compile_definitions(kalidous PRIVATE KALIDOUS_VERSION="v0.0-alpha" KALIDOUS_EXECUTABLE)

# =============================================================================
# Install (Optional)
# =============================================================================
include(GNUInstallDirs)
install(TARGETS kalidous KalidousParse KalidousCore
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# =============================================================================
# Final Info
# =============================================================================
message("")
message(STATUS "💡 Build Summary")
#message(STATUS "   KalidousABI version: ${Kalidous_ABI_VERSION}")  # Commented out as variable is likely unset
#message(STATUS "   LLVM version: ${LLVM_PACKAGE_VERSION}")
if(TARGET Kalidous::Kalidous)
    message(STATUS "   Kalidous::Kalidous target found.")
endif()
message("")