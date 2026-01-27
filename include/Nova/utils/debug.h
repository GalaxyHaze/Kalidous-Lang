//
// Created by dioguabo-rei-delas on 11/9/25.
//

#ifndef NOVA_DEBUG_H
#define NOVA_DEBUG_H

#include "../parse/tokens.h"
#include <vector>
#include <iostream>
namespace nova
{

    // In public header (or separate io.h)
    typedef struct OutputStream {
        void* ctx;
        void (*write)(void* ctx, const char* data, size_t len);
    } OutputStream;

    inline void printTokens(const std::vector<Token>& tokens) {
        std::cout << "Starting the print of Tokens:\n";
        for (const auto& token : tokens) {
            std::cout << token.value << "  ";
        }
    }
}

#endif //NOVA_DEBUG_H