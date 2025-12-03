#ifndef NOVA_TOKENIZER_H
#define NOVA_TOKENIZER_H

#include <string_view>
#include <vector>

#include "helpers.h"
#include "tokens_map.h"
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

    struct LexError {
        std::string message;
        Info info;
    };
    static void addError(std::vector<LexError>& errors, const std::string& msg, const Info& info) noexcept {
        errors.emplace_back(LexError{msg + std::to_string(info.line), info});
    }

    // Enhanced identifier/keyword (unchanged core, but now fallback for any alnum start)
    static void processIdentifierOrKeyword(const char*& current, const char* end,
                                                          std::vector<Token>& tokens, Info& info) noexcept {
        const char* start = current;
        while (has(current, end) && (isAlphaNum(*current) || *current == '_')) {
            consume(info, current);
        }
        const std::string_view lexeme(start, static_cast<size_t>(current - start));
        const TokenType type = lookupToken(lexeme);  // Constexpr magic!
        tokens.emplace_back(type == TokenType::Unknown ? TokenType::Identifier : type, lexeme, info);
    }

    // Enhanced string (escape support: \" \\ \n; unterminated collects error)
    static void processString(const char*& current, const char* end,
                                            std::vector<Token>& tokens, std::vector<LexError>& errors,
                                            Info& info) noexcept {
        const char* start = current;
        consume(info, current);  // Skip "
        while (has(current, end)) {
            if (*current == '\\') {  // Basic escapes
                consume(info, current);
                if (has(current, end)) consume(info, current);
                continue;
            }
            if (*current == '"') {
                consume(info, current);
                tokens.emplace_back(TokenType::String, std::string_view(start, current - start), info);
                return;
            }
            if (*current == '\n') info.newLine();
            consume(info, current);
        }
        addError(errors, "Unterminated string at line ", info);
        tokens.emplace_back(TokenType::String, std::string_view(start, current - start), info);  // Partial
    }

    // Enhanced number: integers, floats, suffixes (u8/i64/f32 etc.), hex(0x)/bin(0b), _ separators
    // Creatively: Parse value at compile-time if small (for constants), but here just lexeme + infer type if suffix
    static void processNumber(const char*& current, const char* end,
                                            std::vector<Token>& tokens, Info& info) noexcept {
        const char* start = current;
        bool isFloat = false;
        bool isHex = false, isBin = false;

        if (*current == '0' && lookAhead(current, end) >= 'a' && lookAhead(current, end) <= 'z') {  // Prefix
            char prefix = std::tolower(lookAhead(current, end));
            if (prefix == 'x') { isHex = true; consume(info, current, 2); }
            else if (prefix == 'b') { isBin = true; consume(info, current, 2); }
        }

        while (has(current, end)) {
            if (*current == '.') { if (isFloat) break; isFloat = true; }  // Only one .
            else if (*current == '_') {}  // Ignore
            else if (!isAlphaNum(*current) || (isHex && !isxdigit(*current)) || (isBin && *current != '0' && *current != '1')) break;
            consume(info, current);
        }

        // Suffix: match known types (longest first, via lookupToken but manual for perf)
        const char* suffixStart = current;
        while (has(current, end) && isAlpha(*current)) consume(info, current);
        if (suffixStart != current) {
            auto suffix = std::string_view(suffixStart, current - suffixStart);
            if (lookupToken(suffix) == TokenType::Type) {  // Reuse! Types are in table
                // Enhance: Could store suffix type in Token, but for now lexeme includes it
            } else {
                current = suffixStart;  // Not a type suffix, rewind
                suffix = {};
            }
        }

        const std::string_view lexeme(start, current - start);
        const TokenType type = isFloat ? TokenType::Float : TokenType::Number;  // Or unify, add Float if needed
        tokens.emplace_back(type, lexeme, info);
    }

    // Removed tryMatchOperator — unified into main loop!

public:
    static void placeholderErrors(const std::vector<LexError>& vector){}

     [[nodiscard]] static auto tokenize(const std::string_view src) noexcept {
        std::vector<Token> tokens;
        std::vector<LexError> errors;  // Collect for user
        tokens.reserve(src.size() / 3 + 1);  // Slightly better avg (accounts for numbers/strings)
        Info info{};
        const char* current = src.data();
        const char* end = src.data() + src.size();

        while (has(current, end)) {
            const char c = *current;
            //info++;  // If Info tracks col, etc.

            // Whitespace
            if (isSpace(c)) {
                if (c == '\n') info.newLine();
                consume(info, current);
                continue;
            }

            // Comments
            if (c == '/' && lookAhead(current, end) == '/') {
                skipSingleLine(info, current, end);
                continue;
            }
            if (c == '/' && lookAhead(current, end) == '*') {
                skipMultiLine(info, current, end);  // Enhance: Collect if unterminated
                if (current >= end) addError(errors, "Unterminated multi-line comment at line ", info);
                continue;
            }

            // Identifier/keyword
            if (isAlpha(c) || c == '_') {
                processIdentifierOrKeyword(current, end, tokens, info);
                continue;
            }

            // Number
            if (isNumeric(c) || (c == '.' && isNumeric(lookAhead(current, end)))) {  // .5
                processNumber(current, end, tokens, info);
                continue;
            }

            // String
            if (c == '"') {
                processString(current, end, tokens, errors, info);
                continue;
            }

            const size_t maxLen = std::min<size_t>(3, end - current);  // Adjust if add longer ops
            std::string_view candidate;
            size_t matchLen = 0;
            TokenType type;
            for (size_t len = maxLen; len > 0; --len) {  // Longest first!
                candidate = std::string_view(current, len);
                type = lookupToken(candidate);
                if (type != TokenType::Unknown) {
                    matchLen = len;
                    break;
                }
            }
            if (matchLen > 0) {
                tokens.emplace_back(type, candidate, info);
                consume(info, current, matchLen);
                continue;
            }

            // Fallback unknown
            addError(errors, std::string("Unknown character '") + c + "' at line ", info);
            tokens.emplace_back(TokenType::Unknown, std::string_view(current, 1), info);
            consume(info, current);
        }

        // Optional: Add EOF token
        tokens.emplace_back(TokenType::End, std::string_view(), info);
        if (!errors.empty())
            placeholderErrors(errors);
        return tokens;
    }
};



#endif // NOVA_TOKENIZER_H