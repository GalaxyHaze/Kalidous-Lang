//
// Created by dioguabo-rei-delas on 11/9/25.
//

#ifndef NOVA_KEYWORDS_H
#define NOVA_KEYWORDS_H
#include <ranges>

#include "tokens.h"
#include "dense_map/unordered_dense_map.h"

using Map = std::pair<const std::string_view, Token>;

static auto KeywordsInitializer() {
    ankerl::unordered_dense::map<std::string_view, Token> keywords;

    return keywords;
}

static auto UnknowInitializer() {
    return Map{};
}

constexpr std::array Operators{
    std::make_pair("+", Token::Plus),
    std::make_pair("-", Token::Minus),
    std::make_pair("*", Token::Multiply),
    std::make_pair("/", Token::Divide),
    std::make_pair("=", Token::Assignment),

    // Comparações
    std::make_pair("==", Token::Equal),
    std::make_pair("!=", Token::NotEqual),
    std::make_pair(">", Token::GreaterThan),
    std::make_pair("<", Token::LessThan),
    std::make_pair(">=", Token::GreaterThanOrEqual),
    std::make_pair("<=", Token::LessThanOrEqual),

    // Lógicos
    std::make_pair("&&", Token::And),
    std::make_pair("||", Token::Or),

    // Agrupadores
    std::make_pair("(", Token::LParen),
    std::make_pair(")", Token::RParen),
    std::make_pair("{", Token::LBrace),
    std::make_pair("}", Token::RBrace),
    std::make_pair("[", Token::LBracket),
    std::make_pair("]", Token::RBracket),

    // Separadores
    std::make_pair(",", Token::Comma),
    std::make_pair(":", Token::Colon),
    std::make_pair(";", Token::Semicolon)
};

constexpr std::array Keywords{
    std::make_pair("i32", Token::Type),
    std::make_pair("public", Token::Modifier),
    std::make_pair("private", Token::Type)
};

const inline auto Unknow = UnknowInitializer();

constexpr Token lookupKeyword(const std::string_view word) noexcept {
    for (auto&& [k, v] : Keywords) {
        if (k == word) return v;
    }
    return Token::Unknown;
}

constexpr Token lookupOperator(const std::string_view word) noexcept {
    for (auto&& [k, v] : Keywords) {
        if (k == word) return v;
    }
    return Token::Unknown;
}

/*constexpr bool hasKeyword(const std::string_view word) noexcept {
    for (const auto &k: Keywords | std::views::keys) {
        if (k == word) return true;
    }
    return false;
}*/

constexpr bool lookupUnknow(const std::string_view word) noexcept {
    for (const auto &k: Keywords | std::views::keys) {
        if (k == word) return true;
    }
    return false;
}

#endif //NOVA_KEYWORDS_H