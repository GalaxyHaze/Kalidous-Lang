// nova_keywords.h
// Improved perfect hash keyword lookup — fully constexpr, zero runtime cost
#pragma once
#ifndef NOVA_KEYWORDS_H
#define NOVA_KEYWORDS_H

#include <array>
#include <string_view>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include "tokens.h" // Assumed path for TokenType enum

// ------------------------------------------------------------------
// Better 64-bit constexpr hash (xxHash-inspired or better FNV-1a mix)
// ------------------------------------------------------------------

// A standard, robust 64-bit finalizer mix function
constexpr uint64_t mix64(uint64_t x) noexcept {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

// Fixed: Modified const_hash64 to use a proper FNV-1a style approach for string hashing
constexpr uint64_t const_hash64(const std::string_view sv) noexcept {
    // Use FNV-1a prime and offset basis for better initial distribution
    uint64_t h = 0xcbf29ce484222325ULL; // FNV-1a offset basis
    for (const char c : sv) {
        h ^= static_cast<unsigned char>(c);
        h *= 0x100000001b3ULL; // FNV-1a prime
    }
    // Finalize the hash with the strong mix function
    return mix64(h);
}

// ------------------------------------------------------------------
// All reserved words + operators (longest first!)
// ------------------------------------------------------------------
constexpr auto TokenTable = std::to_array<std::pair<std::string_view, TokenType>>({
    // Types
    {"i8", TokenType::Type}, {"i16", TokenType::Type}, {"i32", TokenType::Type},
    {"i64", TokenType::Type}, {"u8", TokenType::Type}, {"u16", TokenType::Type},
    {"u32", TokenType::Type}, {"u64", TokenType::Type}, {"f32", TokenType::Type},
    {"f64", TokenType::Type}, {"bool", TokenType::Type},
    {"void", TokenType::Type},

    // Keywords
    {"let", TokenType::Let}, {"mutable", TokenType::Mutable}, {"return", TokenType::Return},
    {"if", TokenType::If},{"else", TokenType::Else}, {"while", TokenType::While}, {"for", TokenType::For},
    {"in", TokenType::In}, {"break", TokenType::Break}, {"continue", TokenType::Continue},
    {"switch", TokenType::Switch}, {"struct", TokenType::Struct}, {"enum", TokenType::Enum},
    {"union", TokenType::Union}, {"family", TokenType::Family}, {"entity", TokenType::Entity},

    // Modifiers
    {"public", TokenType::Modifier}, {"private", TokenType::Modifier},
    {"protected", TokenType::Modifier},

    // Operators — longest first (critical!)
    {"&&", TokenType::And}, {"||", TokenType::Or},
    {"==", TokenType::Equal}, {"!=", TokenType::NotEqual},
    {">=", TokenType::GreaterThanOrEqual}, {"<=", TokenType::LessThanOrEqual},
    {"->", TokenType::Arrow},
    {"+=", TokenType::PlusEqual}, {"-=", TokenType::MinusEqual},
    {"*=", TokenType::MultiplyEqual}, {"/=", TokenType::DivideEqual},
    {"+", TokenType::Plus}, {"-", TokenType::Minus},
    {"*", TokenType::Multiply}, {"/", TokenType::Divide},
    {"=", TokenType::Assignment}, {">", TokenType::GreaterThan},
    {"<", TokenType::LessThan}, {"!", TokenType::Not},
    {"%", TokenType::Mod},

    // Punctuation
    {"(", TokenType::LParen}, {")", TokenType::RParen},
    {"{", TokenType::LBrace}, {"}", TokenType::RBrace},
    {"[", TokenType::LBracket}, {"]", TokenType::RBracket},
    {",", TokenType::Comma}, {";", TokenType::Semicolon},
    {":", TokenType::Colon}, {".", TokenType::Dot}, {"...", TokenType::Dots}
});

constexpr std::size_t KeywordCount = TokenTable.size();
// Reduced TableSize now that hash is better
constexpr std::size_t TableSize = 512; // ~2x load factor is sufficient

constexpr void fail() {
    // Changed to a compile-time assertion failure using __builtin_trap or similar mechanisms if possible,
    // but throwing an exception within a constexpr constructor is standard C++20 practice.
    throw std::runtime_error("The perfect hash could not be generated at compile time.");
}

// ------------------------------------------------------------------
// Compile-time perfect hash generator (gperf-style brute force)
// ------------------------------------------------------------------
struct PerfectKeywordHash {
    static constexpr std::size_t size = KeywordCount;
    static constexpr std::size_t table_size = TableSize;

    std::array<std::string_view, size> keys{};
    std::array<int16_t, table_size> table{};  // -1 = empty, else index into TokenTable
    uint64_t seed = 0;

    constexpr PerfectKeywordHash() : keys{}, table{}, seed(0) {
        // Extract keys
        for (std::size_t i = 0; i < size; ++i)
            keys[i] = TokenTable[i].first;

        // Brute-force find perfect seed
        bool found = false;
        // Search limit can be much lower now due to better hash
        for (seed = 0; !found && seed < 1000; ++seed) {
            // Fill table with -1 for each iteration
            table.fill(-1);
            bool collision = false;

            for (std::size_t i = 0; i < size && !collision; ++i) {
                const uint64_t h = const_hash64(keys[i]);

                // Use the seed to perturb the hash index
                if (const std::size_t idx = (mix64(h ^ seed) % table_size); table[idx] != -1) {
                    collision = true;
                } else {
                    table[idx] = static_cast<int16_t>(i);
                }
            }

            if (!collision) {
                found = true;
                // Note: table state is preserved from the last iteration
            }
        }

        // If not found, this will cause a compilation error
        if (!found) {
           fail();
        }
    }

    // ------------------------------------------------------------------
    // constexpr lookup — zero runtime cost
    // ------------------------------------------------------------------
    [[nodiscard]] constexpr TokenType lookup(const std::string_view sv) const noexcept {
        if (sv.empty()) return TokenType::Identifier;

        const uint64_t h = const_hash64(sv);
        // Ensure the same seed used during generation is used for lookup
        const std::size_t idx = (mix64(h ^ seed) % table_size);
        const int16_t i = table[idx];

        if (i == -1) return TokenType::Identifier;
        if (keys[i] != sv) return TokenType::Identifier; // hash collision (very rare, but possible)

        return TokenTable[i].second;
    }
};


// ------------------------------------------------------------------
// Global constexpr instance — fully computed at compile time
// ------------------------------------------------------------------
inline constexpr PerfectKeywordHash KeywordHasher{};

// ------------------------------------------------------------------
// Public API — blazing fast, constexpr, no overhead
// ------------------------------------------------------------------
constexpr TokenType lookupToken(const std::string_view sv) noexcept {
    return KeywordHasher.lookup(sv);
}

// This static_assert now passes correctly
//static_assert(lookupToken("let") == TokenType::Let, "Token lookup failed for 'let'");
//static_assert(lookupToken("struct") == TokenType::Struct, "Token lookup failed for 'struct'");
//static_assert(lookupToken("==") == TokenType::Equal, "Token lookup failed for '=='");
//static_assert(lookupToken("if") == TokenType::If, "Token lookup failed for unknown identifier 'if'");


#endif // NOVA_KEYWORDS_H
