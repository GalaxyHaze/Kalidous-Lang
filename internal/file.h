//
// Created by al24254 on 05/11/2025.
//

#ifndef NOVA_FILE_H
#define NOVA_FILE_H

#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cctype>
#include <string>
#include <filesystem>
#include <vector>

namespace internal::nova::file {
    namespace fs = std::filesystem;

    // ------------------------------------------------------------
    inline void validateExistence(const fs::path& filePath) {
        if (!fs::exists(filePath)) {
            throw std::runtime_error("File does not exist: " + filePath.string());
        }

        if (!fs::is_regular_file(filePath)) {
            throw std::runtime_error("Path is not a regular file: " + filePath.string());
        }
    }

    // ------------------------------------------------------------
    inline bool compareInsensitiveCase(std::string_view a, std::string_view b) {
        return a.size() == b.size() &&  std::ranges::equal(a, b,
            [](const char ca, const char cb) {
                return
                std::tolower(static_cast<unsigned char>(ca)) ==
                    std::tolower(static_cast<unsigned char>(cb));
        });
    }

    // ------------------------------------------------------------
    inline void validateExtension(const fs::path& filePath, const std::vector<std::string>& validExtensions) {
        const std::string extension = filePath.extension().string();

        for (const auto& ext : validExtensions) {
            if (compareInsensitiveCase(extension, ext)) return;
            //if (extension == ext) return;

        }

        std::string list;
        for (const auto& ext : validExtensions) list += ext + " ";

        throw std::runtime_error("\nError: Invalid extension '" + extension +
                                 "'\nExpected one of: " + list);

    }

    // ------------------------------------------------------------
    [[nodiscard]] inline size_t getFileSize(const fs::path& filePath) {
        validateExistence(filePath);
        return fs::file_size(filePath);
    }

    // ------------------------------------------------------------
    [[nodiscard]] inline std::string readFile(const fs::path& filePath) {
        validateExistence(filePath);

        const size_t fileSize = fs::file_size(filePath);
        if (fileSize == 0) throw std::runtime_error("File is empty: " + filePath.string());

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) throw std::runtime_error("Could not open file: " + filePath.string());

        std::string content;
        content.resize(fileSize);  // aloca memória de uma vez

        file.read(content.data(), static_cast<long>(fileSize));
        if (!file) throw std::runtime_error("Failed to read file: " + filePath.string());

        return content;  // retorna std::string por valor (move será otimizado pelo compilador)
    }


    // ------------------------------------------------------------
    inline std::string trim(std::string_view str) {
        const auto start = std::ranges::find_if_not(str,
                                                    [](const unsigned char c) { return std::isspace(c); });
        const auto end = std::find_if_not(str.rbegin(), str.rend(),
            [](const unsigned char c) { return std::isspace(c); }).base();
        return start < end ? std::string(start, end) : "";
    }

    // ------------------------------------------------------------
    inline void debugInfo(const fs::path& filePath, const std::string& buffer, const size_t lineShown = 10) {
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

        // Show first few lines for large files
        std::istringstream contentStream(buffer);
        std::string line;
        size_t lineCount = 0;
        while (std::getline(contentStream, line) && lineCount < lineShown) {
            std::cout << line << "\n";
            lineCount++;
        }

        if (lineCount < lineShown)
            std::cout << "\n";
        else
            std::cout << "... (truncated)\n";

        std::cout << "----------------\n" << std::endl;
    }

    // ------------------------------------------------------------
    struct FileReadOptions {
        bool debugEnabled = false;
        size_t maxPreviewLines = 10;
        bool validateExtension = true;
        std::vector<std::string> allowedExtensions = {".nova"};

        explicit FileReadOptions(const bool debug = false, const size_t lines = 10, const bool validate = true,
                                 std::vector<std::string> extensions = {".nova"})
            : debugEnabled(debug), maxPreviewLines(lines),
              validateExtension(validate), allowedExtensions(std::move(extensions)) {}
    };

    // ------------------------------------------------------------
    inline auto readSource(const FileReadOptions& options = FileReadOptions{}) {
        std::cout << "Insert your source file:\n";
        std::string src;
        std::getline(std::cin, src);
        src = trim(src);

        const fs::path filePath(src);
        validateExistence(filePath);

        if (options.validateExtension) {
            validateExtension(filePath, options.allowedExtensions);
        }

        auto buffer = readFile(filePath);

        if (options.debugEnabled) {
            debugInfo(filePath, buffer, options.maxPreviewLines);
        }

        return buffer;
    }
}

#endif // NOVA_FILE_H
