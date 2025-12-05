#ifndef NOVA_TOKENIZER_H
#define NOVA_TOKENIZER_H

#include <string_view>
#include <vector>

#include "helpers.h"
#include "tokens_map.h"
#include "tokens.h"

namespace internal {
    class Tokenizer {
        struct LexError {
            std::string_view message;
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
            Info startInfo = info;
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
            errors.emplace_back("Unterminated multi-line comment at line ", startInfo);
        }

        /*static void addError(std::vector<LexError>& errors, const std::string& msg, const Info& info) noexcept {
            errors.emplace_back(LexError{msg + std::to_string(info.line), info});
        }*/

        static void processIdentifier(const char*& current, const char* end,
        std::vector<Token>& tokens, Info& info) noexcept {
            const char* start = current;
            while (has(current, end) && (isAlphaNum(*current) || *current == '_')) {
                consume(info, current);
            }
            const std::string_view lexeme(start, static_cast<size_t>(current - start));
            tokens.emplace_back(TokenType::Identifier, lexeme, info);
        }

        //TODO: Escape sequences, multiline strings etc.
        static void processString(const char*& current, const char* end,
        std::vector<Token>& tokens, std::vector<LexError>& errors,
        Info& info) noexcept
        {
            const auto startInfo = info;
            const char* start = current;
            consume(info, current);  // Skip "
            while (has(current, end)) {
                if (*current == '"') {
                    consume(info, current);
                    tokens.emplace_back(TokenType::String, std::string_view(start, current - start), startInfo);
                    return;
                }
                if (*current == '\n') info.newLine();
                consume(info, current);
            }
            errors.emplace_back("Unterminated string at line ", info);
            tokens.emplace_back(TokenType::String, std::string_view(start, current - start), info);  // Partial
        }

        //TODO: other type of numbers
        static void processNumber(const char*& current, const char* end,
        std::vector<Token>& tokens, Info& info, std::vector<LexError>& errors) noexcept {
            const char* start = current;
            const auto startInfo = info;
            //Hexadecimal
            if (*start != '0' && start[1] != 'x' || start[1] != 'X') {
                consume(info, current, 2);
                auto valid = false;
                while (has(current, end)) {
                    switch (*current) {
                            case '0':
                                case '1':
                                case '2':
                                case '3':
                                case '4':
                                case '5':
                                case '6':
                                case '7':
                                case '8':
                                case '9':
                            case 'A':
                                case 'B':
                                case 'C':
                                case 'D':
                                case 'E':
                                case 'F':
                            case 'a':
                                case 'b':
                                case 'c':
                                case 'd':
                                case 'e':
                                case 'f':
                            valid = true;
                            consume(info, current);
                            break;
                            default:
                            if (!valid)
                                errors.emplace_back("Error: the actual Hexadecimal number isn't valid: ");
                            tokens.emplace_back(TokenType::Hexadecimal, std::string_view(start, current), info);
                            return;
                    }
                }
            }
            //Octal
            if (*start != '0' && start[1] != 'b' || start[1] != 'X') {
                consume(info, current, 2);
                auto valid = false;
                while (has(current, end)) {
                    switch (*current) {
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                            valid = true;
                            consume(info, current);
                            break;
                        default:
                            if (!valid)
                                errors.emplace_back("Error: the actual Octonal number isn't valid: ");
                            tokens.emplace_back(TokenType::Octonal, std::string_view(start, current), info);
                            return;
                    }
                }
            }



            /*bool isFloat = false;
            bool isHex = false, isBin = false;

            if (*current == '0' && lookAhead(current, end) >= 'a' && lookAhead(current, end) <= 'z') {  // Prefix
                if (const char prefix = toLower(lookAhead(current, end)); prefix == 'x') { isHex = true; consume(info, current, 2); }
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
                if (auto suffix = std::string_view(suffixStart, current - suffixStart); lookupToken(suffix) == TokenType::Type) {  // Reuse! Types are in table
                    // Enhance: Could store suffix type in Token, but for now lexeme includes it
                } else {
                    current = suffixStart;  // Not a type suffix, rewind
                    suffix = {};
                }
            }

            const std::string_view lexeme(start, current - start);
            const TokenType type = isFloat ? TokenType::Float : TokenType::Number;  // Or unify, add Float if needed
            tokens.emplace_back(type, lexeme, info);*/
        }

        // Removed tryMatchOperator — unified into main loop!

    public:
        static void showErrors(const std::vector<LexError>& vector) {
            for (const auto&[message, info] : vector) {
                std::cerr << "Lexical Error (line " << info.line << " & column  " << info.index << ") :" << message << '\n';
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


        static bool punctuation(const char* current, const char* end,
                             std::vector<Token>& out,
                             Info& info) noexcept
        {

            for (const size_t len : {3u, 2u, 1u}) {
                std::string_view view = make_view(current, end, len);
                if (view.empty()) continue;

                TokenType t = lookupToken(view);
                if (t == TokenType::Identifier) continue;

                consume(info, current, len);
                out.emplace_back(t, view, info);
                return true;
            }

            return false;
        }



        [[nodiscard]] static auto tokenize(const std::string_view src) noexcept {
            std::vector<Token> tokens;
            std::vector<LexError> errors;  // Collect for user
            tokens.reserve(src.size() / 2 + 1);  // Slightly better avg (accounts for numbers/strings)
            errors.reserve(src.size() / 3);
            Info info{};
            const char* current = src.data();
            const char* end = src.data() + src.size();

            while (has(current, end)) {
                const char c = *current;

                if (isSpace(c)) {
                    if (c == '\n') info.newLine();
                    consume(info, current);
                    continue;
                }

                if (c == '/' && lookAhead(current, end) == '/') {
                    skipSingleLine(info, current, end);
                    continue;
                }

                if (c == '/' && lookAhead(current, end) == '*') {
                    skipMultiLine(info, current, end, errors);
                    if (current >= end) errors.emplace_back("Unterminated multi-line comment at line ", info);
                    continue;
                }

                if (punctuation(current, end, tokens, info))
                    continue;

                // Identifier / variables
                if (isAlpha(c) || c == '_') {
                    processIdentifier(current, end, tokens, info);
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


                addError(errors, std::string("Unknown character '") + c + "' at line ", info);
                tokens.emplace_back(TokenType::Unknown, std::string_view(current, 1), info);
                consume(info, current);
            }

            // Optional: Add EOF token
            tokens.emplace_back(TokenType::End, std::string_view(), info);
            if (!errors.empty())
                showErrors(errors);
            return tokens;
        }
    };
}

#endif // NOVA_TOKENIZER_H