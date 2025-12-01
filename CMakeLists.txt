cmake_minimum_required(VERSION 3.28)
project(Nova)

set(CMAKE_CXX_STANDARD 20)

add_executable(Nova src/main.cpp
        internal/file.h
        internal/tokenizer.h
        internal/core.h
        internal/helpers.h
        internal/tokens.h
        internal/dense_map/unordered_dense_map.h
        internal/dense_map/stl.h
        internal/tokens_map.h
        internal/debug.h
        internal/parser.h
        internal/perfect_hash.h)
