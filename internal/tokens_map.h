//
// Created by dioguabo-rei-delas on 11/9/25.
//

#ifndef NOVA_KEYWORDS_H
#define NOVA_KEYWORDS_H

#include <string_view>
#include <cstdint>
#include "tokens.h"

// ------------------------------------------------------------------
// Compile-time FNV-1a 64-bit hash (constexpr friendly)
// ------------------------------------------------------------------
constexpr uint64_t fnv1a_64(std::string_view s) noexcept {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (const char c : s) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

// ------------------------------------------------------------------
// Single, fast, constexpr lookup for identifiers AND operators
// ------------------------------------------------------------------
constexpr TokenType lookupToken(const std::string_view sv) noexcept {
    if (sv.empty()) return TokenType::Unknown;

    constexpr auto h = [](const std::string_view str) constexpr { return fnv1a_64(str); };

    switch (fnv1a_64(sv)) {

        // ── Types ─────────────────────────────────────
        case h("i8"):      return TokenType::Type;
        case h("i16"):     return TokenType::Type;
        case h("i32"):     return TokenType::Type;
        case h("i64"):     return TokenType::Type;
        case h("u8"):      return TokenType::Type;
        case h("u16"):     return TokenType::Type;
        case h("u32"):     return TokenType::Type;
        case h("u64"):     return TokenType::Type;
        case h("f32"):     return TokenType::Type;
        case h("f64"):     return TokenType::Type;
        case h("bool"):    return TokenType::Type;
        case h("str"):     return TokenType::Type;
        case h("void"):    return TokenType::Type;

        // ── Keywords ──────────────────────────────────
        case h("let"):     return TokenType::Let;
        case h("mutable"):     return TokenType::Mutable;
        case h("return"):   return TokenType::Return;
        case h("if"):      return TokenType::If;
        case h("else"):    return TokenType::Else;
        case h("while"):   return TokenType::While;
        case h("for"):     return TokenType::For;
        case h("in"):      return TokenType::In;
        case h("break"):   return TokenType::Break;
        case h("continue"): return TokenType::Continue;
        case h("switch"):   return TokenType::Switch;
        case h("struct"):   return TokenType::Struct;
        case h("enum"):     return TokenType::Enum;
        case h("union"):    return TokenType::Union;
        case h("family"):   return TokenType::Family;
        case h("entity"):

        // ── Access Modifiers ──────────────────────────
        case h("public"):    return TokenType::Modifier;
        case h("private"):   return TokenType::Modifier;
        case h("protected"): return TokenType::Modifier;

        // ── Operators (longest first!) ────────────────
        case h("&&"):  return TokenType::And;
        case h("||"):  return TokenType::Or;
        case h("=="):  return TokenType::Equal;
        case h("!="):  return TokenType::NotEqual;
        case h(">="):  return TokenType::GreaterThanOrEqual;
        case h("<="):  return TokenType::LessThanOrEqual;
        case h("->"):  return TokenType::Arrow;
        case h("+="):  return TokenType::PlusEqual;
        case h("-="):  return TokenType::MinusEqual;
        case h("*="):  return TokenType::MultiplyEqual;
        case h("/="):  return TokenType::DivideEqual;

        case h("+"):   return TokenType::Plus;
        case h("-"):   return TokenType::Minus;
        case h("*"):   return TokenType::Multiply;
        case h("/"):   return TokenType::Divide;
        case h("="):   return TokenType::Assignment;
        case h(">"):   return TokenType::GreaterThan;
        case h("<"):   return TokenType::LessThan;
        case h("!"):   return TokenType::NotEqual;
        case h("%"):    return TokenType::Mod;

        // ── Punctuation ───────────────────────────────
        case h("("):   return TokenType::LParen;
        case h(")"):   return TokenType::RParen;
        case h("{"):   return TokenType::LBrace;
        case h("}"):   return TokenType::RBrace;
        case h("["):   return TokenType::LBracket;
        case h("]"):   return TokenType::RBracket;
        case h(","):   return TokenType::Comma;
        case h(";"):   return TokenType::Semicolon;
        case h(":"):   return TokenType::Colon;
        case h("."):   return TokenType::Dot;
        case h("..."):  return TokenType::Dots;

        default:
            return TokenType::Identifier;  // Not reserved → normal identifier
    }
}

#endif //NOVA_KEYWORDS_H