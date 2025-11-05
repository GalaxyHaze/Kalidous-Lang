//
// Created by al24254 on 05/11/2025.
//

#ifndef NOVA_FILE_H
#define NOVA_FILE_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

inline std::string_view readSource() {
    std::cout << "Insert your source file:\n";
    std::string src;
    std::cin >> src;

    // Verifica se o arquivo existe
    fs::path filePath(src);

    if (!fs::exists(filePath)) {
            throw std::runtime_error("File does not exist");
    }

    // Verifica se é um arquivo regular (não diretório)
    if (!fs::is_regular_file(filePath)) {
        throw std::runtime_error("Error: Path is not a regular file.");
    }

    // Verifica a extensão do arquivo
    std::string extension = filePath.extension().string();

    // Lista de extensões de arquivos source suportadas
    std::vector<std::string> validExtensions = {".nova" };

    bool isValidExtension = false;
    for (const auto& validExt : validExtensions) {
        if (extension == validExt) {
            isValidExtension = true;
            break;
        }
    }

    if (!isValidExtension) {
       throw std::runtime_error("Error: Extension is not valid.");
    }

    // Abre e lê o arquivo
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open source file." << std::endl;
        //return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    std::cout << "File: " << filePath.filename() << " (" << extension << ")\n";
    std::cout << "Size: " << source.size() << " bytes\n";
    std::cout << "Content:\n\n" << source << std::endl;

    file.close();
}

#endif //NOVA_FILE_H