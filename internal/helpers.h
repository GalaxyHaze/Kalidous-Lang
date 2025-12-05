//
// Created by dioguabo-rei-delas on 11/6/25.
//

#ifndef NOVA_HELPERS_H
#define NOVA_HELPERS_H

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
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
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

inline void lexError(const Info& info, const std::string_view msg) {
    fprintf(stderr, "Error (line %hu): %.*s\n",
            info.line, static_cast<int>(msg.size()), msg.data());
    std::abort();
}

#endif //NOVA_HELPERS_H