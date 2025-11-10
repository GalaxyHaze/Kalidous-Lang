//
// Created by dioguabo-rei-delas on 11/9/25.
//

#ifndef NOVA_DEBUG_H
#define NOVA_DEBUG_H

#include "tokens.h"
#include <vector>
#include <iostream>

inline void printTokens(const std::vector<TokenType>& tokens) {
    std::cout << "Starting the print of Tokens:\n";
    for (const auto& token : tokens) {
        std::cout << token.value << "  ";
    }
}

#endif //NOVA_DEBUG_H