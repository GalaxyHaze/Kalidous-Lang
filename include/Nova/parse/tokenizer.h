#ifndef NOVA_TOKENIZER_H
#define NOVA_TOKENIZER_H

#include <string_view>
#include <vector>
#include <iostream> // Added for std::cerr
#include <cstdlib>  // Added for std::abort
#include <cctype>   // Added for standard character checks

#include "../utils/helpers.h"
#include "tokens_map.h"
#include "tokens.h"

namespace nova {
    using namespace helpers;
    class Tokenizer {
        struct LexError {
            std::string message; // Changed from string_view to string to fix dangling reference bug
            Info info;
        };

        [[nodiscard]] static constexpr bool has(const char* current, const char* end) noexcept {
            return current < end;
        }

        [[nodiscard]] static constexpr char lookAhead(const char *current, const char *end) noexcept {
            const char* pos = current + 1;
            return pos < end ? *pos : '\0';
        }

        static void consume(Info& info, const char*& current, const size_t offset = 1) noexcept {
            current += offset;
            info += offset;
        }

        static void skipSingleLine(Info& info, const char*& current, const char* end) noexcept {
            while (has(current, end) && *current != '\n')
                consume(info, current);
        }

        static void skipMultiLine(Info& info, const char*& current, const char* end, std::vector<LexError>& errors) noexcept {
            const Info startInfo = info;
            consume(info, current, 2); // Skip /*

            while (has(current, end)) {
                if (*current == '*' && lookAhead(current, end) == '/') {
                    consume(info, current, 2); // Skip */
                    return;
                }
                if (*current == '\n')
                    info.newLine();

                consume(info, current);
            }
            // If we reach here, the comment was not terminated
            errors.emplace_back(LexError{"Unterminated multi-line comment starting at line " + std::to_string(startInfo.line), startInfo});
        }

        static void addError(std::vector<LexError>& errors, const std::string& msg, const Info& info) noexcept {
            errors.emplace_back(LexError{msg, info});
        }

        static void processIdentifier(const char*& current, const char* end,
        std::vector<Token>& tokens, Info& info) noexcept {
            const char* start = current;
            while (has(current, end) && (nova::helpers::isAlphaNum(*current) || *current == '_')) {
                consume(info, current);
            }
            const std::string_view lexeme(start, static_cast<size_t>(current - start));

            // Check if it is a keyword
            TokenType type = lookupToken(lexeme);
            if (type == TokenType::Identifier) {
                 // If lookup returned Identifier (or Unknown if that's your default), ensure it's set to Identifier
                 // Assuming lookupToken returns TokenType::Identifier if not found as keyword
                 type = TokenType::Identifier;
            }

            tokens.emplace_back(type, lexeme, info);
        }

        static void processString(const char*& current, const char* end,
        std::vector<Token>& tokens, std::vector<LexError>& errors,
        Info& info) noexcept
        {
            const auto startInfo = info;
            const char* start = current;
            consume(info, current);  // Skip opening "

            while (has(current, end)) {
                if (*current == '"') {
                    consume(info, current); // Skip closing "
                    tokens.emplace_back(TokenType::String, std::string_view(start, current - start), startInfo);
                    return;
                }

                // Basic escape sequence handling (prevent \" from terminating string)
                if (*current == '\\') {
                    consume(info, current); // Skip backslash
                    if (has(current, end)) {
                         consume(info, current); // Skip escaped char
                         continue;
                    }
                }

                if (*current == '\n') info.newLine();
                consume(info, current);
            }

            addError(errors, "Unterminated string at line " + std::to_string(info.line), info);
            tokens.emplace_back(TokenType::String, std::string_view(start, current - start), info);  // Partial
        }

        static void processNumber(const char*& current, const char* end,
        std::vector<Token>& tokens, Info& info, std::vector<LexError>& errors) noexcept {
            const char* start = current;
            const auto startInfo = info;

            bool isHex = false;
            bool isBin = false;
            bool isFloat = false;

            // Check Prefixes
            if (*current == '0' && has(current + 1, end)) {
                char next = nova::helpers::toLower(*(current + 1));
                if (next == 'x') {
                    isHex = true;
                    consume(info, current, 2); // Skip 0x
                } else if (next == 'b') {
                    isBin = true;
                    consume(info, current, 2); // Skip 0b
                }
            }

            // Parse Digits
            while (has(current, end)) {
                char c = *current;

                // Handle separators (e.g. 1_000)
                if (c == '_') {
                    consume(info, current);
                    continue;
                }

                if (isHex) {
                    if (!isxdigit(c)) break;
                } else if (isBin) {
                    if (c != '0' && c != '1') break;
                } else {
                    // Decimal logic
                    if (c == '.') {
                        // Prevent double dots like 1.2.3
                        if (isFloat) break;
                        // Ensure dot is followed by digit
                        if (!has(current + 1, end) || !isdigit(*(current + 1))) break;

                        isFloat = true;
                    } else if (!isdigit(c)) {
                        break;
                    }
                }
                consume(info, current);
            }

            // Determine Token Type
            TokenType type = TokenType::Number; // Default Integer
            if (isHex) type = TokenType::Hexadecimal;
            else if (isBin) type = TokenType::Binary; // Assuming you have this, or Octonal
            else if (isFloat) type = TokenType::Float;

            tokens.emplace_back(type, std::string_view(start, current - start), startInfo);
        }

    public:
        static void showErrors(const std::vector<LexError>& vector) {
            for (const auto& err : vector) {
                std::cerr << "Lexical Error (line " << err.info.line << " & column " << err.info.index << "): " << err.message << '\n';
            }
            std::abort();
        }

        static constexpr std::string_view make_view(const char* start, const char* end, const size_t size) noexcept {
            if (start <= end) {
                if (const auto rem = static_cast<std::size_t>(end - start); rem >= size) {
                    return {start, size};
                }
            }
            return std::string_view{};
        }

        static bool punctuation(const char*& current, const char* end,
                             std::vector<Token>& out,
                             Info& info) noexcept
        {
            // Try 3 chars, then 2, then 1
            for (const size_t len : {3u, 2u, 1u}) {
                std::string_view view = make_view(current, end, len);
                if (view.empty()) continue;

                TokenType t = lookupToken(view);
                if (t == TokenType::Identifier) continue; // lookupToken returns Identifier for unknown strings

                consume(info, current, len);
                out.emplace_back(t, view, info);
                return true;
            }
            return false;
        }

        [[nodiscard]] static auto tokenize(const std::string_view src) noexcept {
            std::vector<Token> tokens;
            std::vector<LexError> errors;
            tokens.reserve(src.size() / 2 + 1);
            errors.reserve(src.size() / 10); // Fewer errors expected usually

            Info info{};
            const char* current = src.data();
            const char* end = src.data() + src.size();

            while (has(current, end)) {
                const char c = *current;

                if (nova::helpers::isSpace(c)) {
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
                    skipMultiLine(info, current, end, errors);
                    continue;
                }

                // Punctuation / Operators
                if (punctuation(current, end, tokens, info))
                    continue;

                // Identifier / Keywords
                if (nova::helpers::isAlpha(c) || c == '_') {
                    processIdentifier(current, end, tokens, info);
                    continue;
                }

                // Numbers
                if (nova::helpers::isNumeric(c) || (c == '.' && nova::helpers::isNumeric(lookAhead(current, end)))) {
                    processNumber(current, end, tokens, info, errors);
                    continue;
                }

                // Strings
                if (c == '"') {
                    processString(current, end, tokens, errors, info);
                    continue;
                }

                // Unknown Character
                addError(errors, std::string("Unknown character '") + c + "' at line " + std::to_string(info.line), info);
                tokens.emplace_back(TokenType::Unknown, std::string_view(current, 1), info);
                consume(info, current);
            }

            tokens.emplace_back(TokenType::End, std::string_view(), info);

            if (!errors.empty())
                showErrors(errors);

            return tokens;
        }
    };
}

#endif // NOVA_TOKENIZER_H