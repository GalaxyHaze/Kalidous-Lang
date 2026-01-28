//
// Created by GalaxyHaze
//

#ifndef NOVA_HELPERS_H
#define NOVA_HELPERS_H
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../parse/tokens.h"

namespace nova::helpers
{
    constexpr bool isNewLine(const char c)
    {
        return (c == '\n' || c == '\r' || c == '\t');
    }

    constexpr bool isAlphaNum(const char c) noexcept {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') ||
               c == '_';
    }

    constexpr char toLower(const char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(static_cast<int8_t>(c) + static_cast<uint8_t>(32)) : c;
    }

    constexpr bool isAlpha(const char c) noexcept {
        return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z';
    }

    constexpr bool isNumeric(const char c) noexcept {
        return c >= '0' && c <= '9';
    }

    constexpr bool isSpace(const char c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    inline void lexError(const NovaInfo& info, const std::string_view msg) {
        fprintf(stderr, "Error (line %lu): %.*s\n",
                info.line, static_cast<int>(msg.size()), msg.data());
        std::abort();
    }
}

#endif //NOVA_HELPERS_H