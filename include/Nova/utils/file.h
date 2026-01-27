// include/Nova/utils/file_utils.hpp
#ifndef NOVA_UTILS_FILE_UTILS_HPP
#define NOVA_UTILS_FILE_UTILS_HPP

#include "../memory/file.h"
#include "../memory/arena.h"
#include "../memory/Arena.h"
#include <filesystem>
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <ranges>

namespace nova::file {
    namespace fs = std::filesystem;

    // Reuse your existing helpers (they’re great!)
    inline void validateExistence(const fs::path& filePath) {
        if (!nova_file_exists(filePath.c_str())) {
            throw std::runtime_error("File does not exist: " + filePath.string());
        }
        if (!nova_file_is_regular(filePath.c_str())) {
            throw std::runtime_error("Path is not a regular file: " + filePath.string());
        }
    }

    inline bool compareInsensitiveCase(std::string_view a, std::string_view b) {
        return a.size() == b.size() && std::ranges::equal(a, b,
            [](unsigned char ca, unsigned char cb) {
                return std::tolower(ca) == std::tolower(cb);
            });
    }

    inline void validateExtension(const fs::path& filePath, const std::vector<std::string>& validExtensions) {
        std::string actual_ext = filePath.extension().string();
        for (const auto& ext : validExtensions) {
            if (compareInsensitiveCase(actual_ext, ext)) return;
        }
        std::string list;
        for (const auto& ext : validExtensions) list += ext + " ";
        throw std::runtime_error("\nError: Invalid extension '" + actual_ext +
                                 "'\nExpected one of: " + list);
    }

    // 🔁 NEW: Read file using Arena (no std::string allocation in core)
    inline std::pair<char*, size_t> readFileToArena(const Arena& arena, const fs::path& filePath) {
        validateExistence(filePath);
        size_t size = 0;
        char* data = nova_load_file_to_arena(arena.get(), filePath.c_str(), &size);
        if (!data) throw std::runtime_error("Failed to load file into arena: " + filePath.string());
        return {data, size};
    }

    // Keep debugInfo, trim, etc. — they’re fine for tooling
    inline std::string trim(std::string_view str) {
        const auto start = std::ranges::find_if_not(str, [](const unsigned char c) { return std::isspace(c); });
        const auto end = std::find_if_not(str.rbegin(), str.rend(), [](const unsigned char c) { return std::isspace(c); }).base();
        return start < end ? std::string(start, end) : "";
    }

    inline void debugInfo(const fs::path& filePath, std::string_view buffer, const size_t lineShown = 10)
    {
        const std::string extension = filePath.extension().string();

        std::cout << "=== File Information ===\n";
        std::cout << "Filename: " << filePath.filename() << "\n";
        std::cout << "Extension: " << extension << "\n";
        std::cout << "Size: " << buffer.size() << " bytes\n";
        size_t lines = std::ranges::count(buffer, '\n');
        if (!buffer.empty() && buffer.back() != '\n') ++lines;
        std::cout << "Lines: " << lines << "\n";
        std::cout << "Path: " << filePath << "\n";
        std::cout << "Content Preview:\n";
        std::cout << "----------------\n\n";

        std::string_view remaining{buffer};
        size_t lineCount = 0;

        while (!remaining.empty() && lineCount < lineShown) {
            // 1. Find the end of the current line
            const size_t pos = remaining.find('\n');

            // 2. Extract the line as a view (no copying/allocation happens here!)
            const std::string_view line = (pos == std::string_view::npos)
                                     ? remaining
                                     : remaining.substr(0, pos);

            // 3. Print it
            std::cout << line << "\n";

            // 4. Shrink the window: move past the line we just printed
            if (pos == std::string_view::npos) {
                remaining = {}; // End of data
            } else {
                remaining.remove_prefix(pos + 1); // Skip the content + the '\n'
            }

            lineCount++;
        }

        if (lineCount >= lineShown && !remaining.empty()) {
            std::cout << "... (truncated)\n";
        } else {
            std::cout << "\n";
        }
    }

    struct FileReadOptions {
        bool debugEnabled = false;
        size_t maxPreviewLines = 10;
        bool validateExtension = true;
        std::vector<std::string> allowedExtensions = {".nova"};
        explicit FileReadOptions(bool debug = false, size_t lines = 10, bool validate = true,
                                 std::vector<std::string> extensions = {".nova"})
            : debugEnabled(debug), maxPreviewLines(lines),
              validateExtension(validate), allowedExtensions(std::move(extensions)) {}
    };

    // CLI helper: uses Arena internally
    inline std::pair<char*, size_t> readSource(const Arena& arena, const FileReadOptions& options = FileReadOptions{}) {
        std::cout << "Insert your source file:\n";
        std::string src;
        std::getline(std::cin, src);
        src = trim(src);

        fs::path filePath(src);
        validateExistence(filePath);

        if (options.validateExtension) {
            validateExtension(filePath, options.allowedExtensions);
        }

        auto [data, size] = readFileToArena(arena, filePath);

        if (options.debugEnabled) {
            debugInfo(filePath, std::string_view(data, size), options.maxPreviewLines);
        }

        return {data, size};
    }
}

#endif // NOVA_UTILS_FILE_UTILS_HPP