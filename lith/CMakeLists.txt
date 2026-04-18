cmake_minimum_required(VERSION 3.20)
project(lith LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable strict warnings as errors
option(ENABLE_WERROR "Treat warnings as errors" OFF)

if (MSVC)
    add_compile_options(/W4 /wd4100)
    if(ENABLE_WERROR)
        add_compile_options(/WX)
    endif()
else ()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(ENABLE_WERROR)
        add_compile_options(-Werror)
    endif()
endif()

add_executable(lith
    src/main.cpp
    src/tokenizer.cpp
)

target_include_directories(lith PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
