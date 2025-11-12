#ifndef NOVA_TOKENIZER_H
#define NOVA_TOKENIZER_H

#include <string_view>
#include <vector>

#include "helpers.h"
#include "maps.h"
#include "tokens.h"

class Tokenizer {
    [[nodiscard]] static constexpr bool has(const char* current, const char* end) noexcept {
        return current < end;
    }

    [[nodiscard]] static constexpr char lookAhead(const char *current, const char *end) noexcept {
        const char* pos = current + 1;
        return pos < end ? *pos : '\0';
    }

    static void consume(Info& info, const char*& current, size_t offset = 1) noexcept {
        current += offset;
        info += offset;
    }

    static void skipMultiLine(Info& info, const char*& current, const char* end) noexcept {
        consume(info, current, 2);

        while (has(current, end)) {
            if (*current == '*' && lookAhead(current, end) == '/') {
                consume(info, current, 2);
                return;
            }

            if (*current == '\n')
                info.newLine();

            consume(info, current);
        }

        lexError(info, "Unterminated multi-line comment at line ");
    }

    static void skipSingleLine(Info& info, const char*& current, const char* end) noexcept {
        while (has(current, end) && *current != '\n')
            consume(info, current);
    }

    static bool tryMatchOperator(const char*& current, const char* end,
                                 std::vector<TokenType>& tokens, Info& info) noexcept {
        // Try 2-character operators first
        if (current + 1 < end) {
            const std::string_view twoCharOp(current, 2);
            if (const auto token = lookupOperator(twoCharOp); token != Token::Unknown) {
                tokens.emplace_back(token, twoCharOp, info);
                consume(info, current, 2);
                return true;
            }
        }

        // Try 1-character operators
        const std::string_view oneCharOp(current, 1);
        if (const auto token = lookupOperator(oneCharOp); token != Token::Unknown) {
            tokens.emplace_back(token, oneCharOp, info);
            consume(info, current);
            return true;
        }

        return false;
    }

public:
    static void processIdentifierAndKeyword(const char*& current, const char* end,
                                            std::vector<TokenType>& tokens, Info& info) noexcept {
        const char* const lexemeStart = current;

        while (has(current, end) &&
               (isAlphaNum(*current) || *current == '_')) {
            consume(info, current);
        }

        const std::string_view lexeme(lexemeStart, static_cast<size_t>(current - lexemeStart));

        if (const auto keyword = lookupKeyword(lexeme); keyword != Token::Unknown) {
            tokens.emplace_back(keyword, lexeme, info);
            return;
        }

        tokens.emplace_back(Token::Identifier, lexeme, info);
    }

    static void processString(const char*& current, const char* end,
                              std::vector<TokenType>& tokens, Info& info) noexcept {
        const char* const lexemeStart = current;
        consume(info, current); // skip opening quote
        bool ended = false;

        while (has(current, end)) {
            if (*current == '"') {
                ended = true;
                consume(info, current);
                break;
            }

            if (*current == '\n')
                info.newLine();

            consume(info, current);
        }

        if (!ended)
            lexError(info, "Unterminated string at line ");

        const std::string_view lexeme(lexemeStart, static_cast<size_t>(current - lexemeStart));
        tokens.emplace_back(Token::String, lexeme, info);
    }

    static void processNumber(const char*& current, const char * end, std::vector<TokenType>& tokens, Info info) {
        const auto lexemeStart = current;
        consume(info, current);
        while (has(current, end)) {
            if (isNumeric(*current) || *current == '.' || *current == '_' ) {
                consume(info, current);
                continue;
            }
            break;
        }
        const auto lexeme = std::string_view(lexemeStart, static_cast<size_t>(current - lexemeStart));
        tokens.emplace_back(Token::Number, lexeme, info);
    }

    [[nodiscard]] static auto tokenize(const std::string_view src) noexcept {
        std::vector<TokenType> tokens;
        tokens.reserve(src.size() / 3);
        Info info{};

        const char* current = src.data();
        const char* end = src.data() + src.size();

        while (current < end) {
            const char c = *current;

            // Whitespace
            if (isSpace(c)) {
                if (c == '\n')
                    info.newLine();

                consume(info, current);
                continue;
            }

            // Single-line comments
            if (c == '/' && lookAhead(current, end) == '/') {
                skipSingleLine(info, current, end);
                continue;
            }

            // Multi-line comments
            if (c == '/' && lookAhead(current, end) == '*') {
                skipMultiLine(info, current, end);
                continue;
            }

            // Identifiers / keywords
            if (isAlpha(c) || c == '_') {
                processIdentifierAndKeyword(current, end, tokens, info);
                continue;
            }

            // Numbers
            if (isNumeric(c)) {
                processNumber(current, end, tokens, info);
            }

            // Strings
            if (c == '"') {
                processString(current, end, tokens, info);
                continue;
            }

            // Operators
            if (tryMatchOperator(current, end, tokens, info)) {
                continue;
            }

            // Unknown token
            tokens.emplace_back(Token::Unknown, std::string_view(current, 1), info);
            consume(info, current);
        }

        return tokens;
    }
};

#endif // NOVA_TOKENIZER_H
