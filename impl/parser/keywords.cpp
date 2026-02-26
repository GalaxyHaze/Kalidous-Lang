// impl/parser/keywords.cpp
#include "Kalidous/kalidous.h"
#include <string_view>
#include <array>

// ============================================================================
// Hash functions
// ============================================================================

static constexpr uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static constexpr uint64_t hash64(const std::string_view sv) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (const unsigned char c : sv) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    return mix64(h);
}

// ============================================================================
// Keyword + operator table
// ============================================================================

static constexpr auto TokenTable = std::to_array<std::pair<std::string_view, KalidousTokenType>>({
    // --- Tipos primitivos ---------------------------------------------------
    {"i8",   KALIDOUS_TOKEN_TYPE}, {"i16",  KALIDOUS_TOKEN_TYPE}, {"i32",  KALIDOUS_TOKEN_TYPE},
    {"i64",  KALIDOUS_TOKEN_TYPE}, {"u8",   KALIDOUS_TOKEN_TYPE}, {"u16",  KALIDOUS_TOKEN_TYPE},
    {"u32",  KALIDOUS_TOKEN_TYPE}, {"u64",  KALIDOUS_TOKEN_TYPE}, {"f32",  KALIDOUS_TOKEN_TYPE},
    {"f64",  KALIDOUS_TOKEN_TYPE}, {"bool", KALIDOUS_TOKEN_TYPE}, {"void", KALIDOUS_TOKEN_TYPE},

    // --- Declarações de tipo ------------------------------------------------
    {"type",      KALIDOUS_TOKEN_TYPE},
    {"struct",    KALIDOUS_TOKEN_STRUCT},
    {"component", KALIDOUS_TOKEN_COMPONENT},
    {"enum",      KALIDOUS_TOKEN_ENUM},
    {"union",     KALIDOUS_TOKEN_UNION},
    {"family",    KALIDOUS_TOKEN_FAMILY},
    {"entity",    KALIDOUS_TOKEN_ENTITY},
    {"trait",     KALIDOUS_TOKEN_TRAIT},
    {"typedef",   KALIDOUS_TOKEN_TYPEDEF},
    {"implement", KALIDOUS_TOKEN_IMPLEMENT},

    // --- Bindings / modificadores de ownership ------------------------------
    {"let",        KALIDOUS_TOKEN_LET},
    {"var",        KALIDOUS_TOKEN_VAR},
    {"auto",       KALIDOUS_TOKEN_AUTO},
    {"const",      KALIDOUS_TOKEN_CONST},
    {"mut",        KALIDOUS_TOKEN_MUTABLE},   // 'mutable' na ABI, 'mut' no fonte
    {"global",     KALIDOUS_TOKEN_GLOBAL},
    {"persistent", KALIDOUS_TOKEN_PERSISTENT},
    {"local",      KALIDOUS_TOKEN_LOCAL},
    {"lend",       KALIDOUS_TOKEN_LEND},
    {"shared",     KALIDOUS_TOKEN_SHARED},
    {"view",       KALIDOUS_TOKEN_VIEW},
    {"unique",     KALIDOUS_TOKEN_UNIQUE},

    // --- Modificadores de acesso --------------------------------------------
    {"public",    KALIDOUS_TOKEN_MODIFIER},
    {"private",   KALIDOUS_TOKEN_MODIFIER},
    {"protected", KALIDOUS_TOKEN_MODIFIER},

    // --- Controle de fluxo --------------------------------------------------
    {"if",       KALIDOUS_TOKEN_IF},
    {"else",     KALIDOUS_TOKEN_ELSE},
    {"for",      KALIDOUS_TOKEN_FOR},
    {"in",       KALIDOUS_TOKEN_IN},
    {"switch",   KALIDOUS_TOKEN_SWITCH},
    {"return",   KALIDOUS_TOKEN_RETURN},
    {"break",    KALIDOUS_TOKEN_BREAK},
    {"continue", KALIDOUS_TOKEN_CONTINUE},
    {"goto",     KALIDOUS_TOKEN_GOTO},
    {"marker",   KALIDOUS_TOKEN_MARKER},
    {"scene",    KALIDOUS_TOKEN_SCENE},
    {"end",      KALIDOUS_TOKEN_END},

    // --- Concorrência -------------------------------------------------------
    {"spawn",  KALIDOUS_TOKEN_SPAWN},
    {"joined", KALIDOUS_TOKEN_JOINED},
    {"await",  KALIDOUS_TOKEN_AWAIT},

    // --- Tratamento de erros ------------------------------------------------
    {"try",   KALIDOUS_TOKEN_TRY},
    {"catch", KALIDOUS_TOKEN_CATCH},
    {"must",  KALIDOUS_TOKEN_MUST},   // o '!' subsequente é semântico, resolvido pelo Parser

    // --- Operadores multi-caractere -----------------------------------------
    {"&&", KALIDOUS_TOKEN_AND},
    {"||", KALIDOUS_TOKEN_OR},
    {"==", KALIDOUS_TOKEN_EQUAL},
    {"!=", KALIDOUS_TOKEN_NOT_EQUAL},
    {">=", KALIDOUS_TOKEN_GREATER_THAN_OR_EQUAL},
    {"<=", KALIDOUS_TOKEN_LESS_THAN_OR_EQUAL},
    {"->", KALIDOUS_TOKEN_ARROW},
    {"+=", KALIDOUS_TOKEN_PLUS_EQUAL},
    {"-=", KALIDOUS_TOKEN_MINUS_EQUAL},
    {"*=", KALIDOUS_TOKEN_MULTIPLY_EQUAL},
    {"/=", KALIDOUS_TOKEN_DIVIDE_EQUAL},
    {":=", KALIDOUS_TOKEN_DECLARATION},
    {"...",KALIDOUS_TOKEN_DOTS},

    // --- Operadores simples -------------------------------------------------
    {"+", KALIDOUS_TOKEN_PLUS},
    {"-", KALIDOUS_TOKEN_MINUS},
    {"*", KALIDOUS_TOKEN_MULTIPLY},   // ponteiro vs aritmético resolvido pelo Parser
    {"/", KALIDOUS_TOKEN_DIVIDE},
    {"%", KALIDOUS_TOKEN_MOD},
    {"=", KALIDOUS_TOKEN_ASSIGNMENT},
    {"<", KALIDOUS_TOKEN_LESS_THAN},
    {">", KALIDOUS_TOKEN_GREATER_THAN},
    {"!", KALIDOUS_TOKEN_BANG},
    {"?", KALIDOUS_TOKEN_QUESTION},

    // --- Delimitadores ------------------------------------------------------
    {"(", KALIDOUS_TOKEN_LPAREN},  {")", KALIDOUS_TOKEN_RPAREN},
    {"{", KALIDOUS_TOKEN_LBRACE},  {"}", KALIDOUS_TOKEN_RBRACE},
    {"[", KALIDOUS_TOKEN_LBRACKET},{"]", KALIDOUS_TOKEN_RBRACKET},
    {",", KALIDOUS_TOKEN_COMMA},   {";", KALIDOUS_TOKEN_SEMICOLON},
    {":", KALIDOUS_TOKEN_COLON},   {".", KALIDOUS_TOKEN_DOT},
});

// ============================================================================
// Perfect hash (compile-time)
// ============================================================================

constexpr size_t N           = TokenTable.size();
constexpr size_t BucketCount = 128;  // aumentado pra acomodar tabela maior
constexpr size_t TableSize   = 256;

namespace {
    struct PerfectHash {
        std::array<int16_t, TableSize> table{};
        std::array<uint8_t, BucketCount> bucketSeed{};

        constexpr PerfectHash() {
            table.fill(-1);
            bucketSeed.fill(0);

            std::array<size_t, N> bucket{};
            for (size_t i = 0; i < N; ++i)
                bucket[i] = hash64(TokenTable[i].first) % BucketCount;

            std::array<size_t, BucketCount> counts{};
            std::array<std::array<uint16_t, 16>, BucketCount> items{};
            for (size_t i = 0; i < N; ++i) {
                size_t b = bucket[i];
                items[b][counts[b]++] = static_cast<uint16_t>(i);
            }

            for (size_t b = 0; b < BucketCount; ++b) {
                if (counts[b] <= 1) {
                    if (counts[b] == 1) {
                        size_t i   = items[b][0];
                        size_t idx = hash64(TokenTable[i].first) % TableSize;
                        table[idx] = static_cast<int16_t>(i);
                    }
                    continue;
                }

                for (uint8_t seed = 0; seed < 255; ++seed) {
                    bool collision = false;
                    std::array<size_t, 16> used{};
                    for (size_t k = 0; k < counts[b]; ++k) {
                        size_t i   = items[b][k];
                        size_t idx = mix64(hash64(TokenTable[i].first) ^ seed) % TableSize;
                        for (size_t j = 0; j < k; ++j) {
                            if (used[j] == idx) { collision = true; break; }
                        }
                        if (collision) break;
                        // verifica também contra entradas já fixadas de outros buckets
                        if (table[idx] != -1) { collision = true; break; }
                        used[k] = idx;
                    }
                    if (!collision) {
                        bucketSeed[b] = seed;
                        for (size_t k = 0; k < counts[b]; ++k) {
                            size_t i   = items[b][k];
                            size_t idx = mix64(hash64(TokenTable[i].first) ^ seed) % TableSize;
                            table[idx] = static_cast<int16_t>(i);
                        }
                        break;
                    }
                }
            }
        }

        [[nodiscard]] constexpr KalidousTokenType lookup(const std::string_view sv) const {
            if (sv.empty()) return KALIDOUS_TOKEN_IDENTIFIER;

            const uint64_t h    = hash64(sv);
            const size_t   b    = h % BucketCount;
            const uint8_t  seed = bucketSeed[b];

            const size_t idx = (countsForBucket(b) <= 1)
                ? (h % TableSize)
                : (mix64(h ^ seed) % TableSize);

            const int16_t id = table[idx];
            if (id < 0) return KALIDOUS_TOKEN_IDENTIFIER;
            return (TokenTable[id].first == sv)
                ? TokenTable[id].second
                : KALIDOUS_TOKEN_IDENTIFIER;
        }

    private:
        static constexpr size_t countsForBucket(size_t b) {
            size_t c = 0;
            for (size_t i = 0; i < N; ++i)
                if ((hash64(TokenTable[i].first) % BucketCount) == b)
                    ++c;
            return c;
        }
    };

    constexpr auto g_hasher = PerfectHash{};
}

// ============================================================================
// C API
// ============================================================================

extern "C" KalidousTokenType kalidous_lookup_keyword(const char* str, const size_t len) {
    if (!str || len == 0) return KALIDOUS_TOKEN_IDENTIFIER;
    return g_hasher.lookup(std::string_view(str, len));
}
