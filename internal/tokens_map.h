#pragma once
#ifndef NOVA_KEYWORDS_H
#define NOVA_KEYWORDS_H

#include <array>
#include <string_view>
#include <cstdint>
#include "tokens.h"

// ------------------------------------------------------------
// Hash Functions
// ------------------------------------------------------------
constexpr uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

constexpr uint64_t hash64(const std::string_view sv) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (const unsigned char c : sv) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    return mix64(h);
}

// ------------------------------------------------------------
// Keyword Table
// ------------------------------------------------------------
constexpr auto TokenTable = std::to_array<std::pair<std::string_view, TokenType>>({
    {"i8", TokenType::Type}, {"i16", TokenType::Type}, {"i32", TokenType::Type},
    {"i64", TokenType::Type}, {"u8", TokenType::Type}, {"u16", TokenType::Type},
    {"u32", TokenType::Type}, {"u64", TokenType::Type}, {"f32", TokenType::Type},
    {"f64", TokenType::Type}, {"bool", TokenType::Type},
    {"void", TokenType::Type},

    {"let", TokenType::Let}, {"mutable", TokenType::Mutable}, {"return", TokenType::Return},
    {"if", TokenType::If},{"else", TokenType::Else}, {"while", TokenType::While},
    {"for", TokenType::For}, {"in", TokenType::In}, {"break", TokenType::Break},
    {"continue", TokenType::Continue}, {"switch", TokenType::Switch},
    {"struct", TokenType::Struct}, {"enum", TokenType::Enum}, {"union", TokenType::Union},
    {"family", TokenType::Family}, {"entity", TokenType::Entity},

    {"public", TokenType::Modifier}, {"private", TokenType::Modifier},
    {"protected", TokenType::Modifier},

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

    {"(", TokenType::LParen}, {")", TokenType::RParen},
    {"{", TokenType::LBrace}, {"}", TokenType::RBrace},
    {"[", TokenType::LBracket}, {"]", TokenType::RBracket},
    {",", TokenType::Comma}, {";", TokenType::Semicolon},
    {":", TokenType::Colon}, {".", TokenType::Dot}, {"...", TokenType::Dots}
});

constexpr size_t N = TokenTable.size();

// ------------------------------------------------------------
// Perfect Hash using 2-level bucket seeds (CHD-like)
// ------------------------------------------------------------
constexpr size_t BucketCount = 64; // good for ~50–200 keywords
constexpr size_t TableSize = 128; // must be >= N

struct PerfectKeywordHash {
    std::array<int16_t, TableSize> table{};
    std::array<uint8_t, BucketCount> bucketSeed{}; // seed per bucket

    constexpr PerfectKeywordHash() {
        table.fill(-1);
        bucketSeed.fill(0);

        // 1. Build buckets
        std::array<uint8_t, N> bucket{};
        for (size_t i = 0; i < N; ++i)
            bucket[i] = hash64(TokenTable[i].first) % BucketCount;

        // 2. Group indices by bucket
        std::array<size_t, BucketCount> counts{};
        counts.fill(0);

        std::array<std::array<uint16_t, 8>, BucketCount> bucketItems{};
        for (size_t i = 0; i < N; ++i) {
            size_t b = bucket[i];
            bucketItems[b][counts[b]++] = static_cast<uint16_t>(i);
        }

        // 3. For each bucket: find a seed that avoids collisions
        for (size_t b = 0; b < BucketCount; ++b) {
            if (counts[b] <= 1) {
                // trivial case
                if (counts[b] == 1) {
                    const size_t i = bucketItems[b][0];
                    const size_t idx = hash64(TokenTable[i].first) % TableSize;
                    table[idx] = static_cast<int16_t>(i);
                }
                continue;
            }

            // try seeds 0..255
            for (uint8_t seed = 0; seed < 255; ++seed) {
                bool collision = false;
                // temp storage for indices
                std::array<int, 8> used{};
                for (size_t k = 0; k < counts[b]; ++k) {
                    const size_t i = bucketItems[b][k];
                    const size_t idx = mix64(hash64(TokenTable[i].first) ^ seed) % TableSize;

                    if (table[idx] != -1) {
                        collision = true;
                        break;
                    }
                    used[k] = static_cast<int>(idx);
                }
                if (!collision) {
                    // commit
                    bucketSeed[b] = seed;
                    for (size_t k = 0; k < counts[b]; ++k) {
                        table[used[k]] = static_cast<int16_t>(bucketItems[b][k]);
                    }
                    break;
                }
            }
        }
    }

    [[nodiscard]] constexpr TokenType lookup(const std::string_view sv) const noexcept {
        if (sv.empty()) return TokenType::Identifier;

        const uint64_t h = hash64(sv);
        const size_t b = h % BucketCount;
        const uint8_t seed = bucketSeed[b];

        const size_t idx =
            (countsForBucket(b) <= 1)
              ? (h % TableSize)
              : (mix64(h ^ seed) % TableSize);

        const int16_t id = table[idx];
        if (id == -1) return TokenType::Identifier;
        return (TokenTable[id].first == sv)
             ? TokenTable[id].second
             : TokenType::Identifier;
    }

    // tiny helper to detect whether a bucket is multi-key
    static constexpr size_t countsForBucket(const size_t b) {
        size_t c = 0;
        for (size_t i = 0; i < N; ++i)
            if ((hash64(TokenTable[i].first) % BucketCount) == b)
                ++c;
        return c;
    }
};

inline constexpr PerfectKeywordHash KeywordHasher{};

constexpr TokenType lookupToken(const std::string_view sv) noexcept {
    return KeywordHasher.lookup(sv);
}


#endif