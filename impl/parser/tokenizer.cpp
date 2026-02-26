// impl/parser/tokenizer.cpp
#include "Kalidous/kalidous.h"
#include <string_view>
#include <vector>
#include <cstring>
#include <iostream>

#define KALIDOUS_NEWLINE(loc) do { \
    (loc)->line++;             \
    (loc)->index = 0;          \
} while(0)

#define MAX_ERRORS 50

namespace kalidous::detail {

struct LexError {
    const char*   msg;
    KalidousSourceLoc info;
};

// ── Arena List Implementation ─────────────────────────────────────────────────
// Grows dynamically in pages/chunks within the arena

struct TokenPage {
    size_t              count;
    size_t              capacity;
    KalidousToken*      data;
    TokenPage*          next;
};

struct TokenList {
    TokenPage*  head;
    TokenPage*  tail;
    size_t      page_capacity; // How many tokens per page
    size_t      total_tokens;  // Total count across all pages
};

static void token_list_init(TokenList* list, const KalidousArena* arena, size_t init_cap) {
    list->head = nullptr;
    list->tail = nullptr;
    list->total_tokens = 0;
    list->page_capacity = init_cap;
    (void)arena; // Unused for now, but kept for consistency
}

static void token_list_push(TokenList* list, KalidousArena* arena, const KalidousToken& token) {
    // If no page or current page is full, allocate a new one
    if (!list->tail || list->tail->count >= list->tail->capacity) {
        auto* new_page = static_cast<TokenPage*>(kalidous_arena_alloc(arena, sizeof(TokenPage)));
        if (!new_page) return; // OOM

        // Exponential growth strategy for page size
        size_t next_cap = list->page_capacity;
        if (list->tail) next_cap = list->tail->capacity * 2;

        new_page->data = static_cast<KalidousToken*>(kalidous_arena_alloc(arena, sizeof(KalidousToken) * next_cap));
        if (!new_page->data) return; // OOM

        new_page->count = 0;
        new_page->capacity = next_cap;
        new_page->next = nullptr;

        if (list->tail) {
            list->tail->next = new_page;
            list->tail = new_page;
        } else {
            list->head = new_page;
            list->tail = new_page;
        }
    }

    // Add token to current tail page
    list->tail->data[list->tail->count++] = token;
    list->total_tokens++;
}

// Flattens the linked list of pages into a single contiguous array in the arena
static KalidousToken* token_list_flatten(const TokenList* list, KalidousArena* arena, size_t* out_count) {
    if (list->total_tokens == 0) {
        *out_count = 0;
        // Allocate a dummy END token so the stream is never null if empty
        auto* dummy = static_cast<KalidousToken*>(kalidous_arena_alloc(arena, sizeof(KalidousToken)));
        if(dummy) {
            dummy->type = KALIDOUS_TOKEN_END;
            dummy->lexeme = {nullptr, 0};
            dummy->loc = {0, 1};
        }
        return dummy;
    }

    const size_t total_size = sizeof(KalidousToken) * list->total_tokens;
    auto* buf = static_cast<KalidousToken*>(kalidous_arena_alloc(arena, total_size));
    if (!buf) return nullptr;

    KalidousToken* cursor = buf;
    TokenPage* page = list->head;

    while (page) {
        std::memcpy(cursor, page->data, sizeof(KalidousToken) * page->count);
        cursor += page->count;
        page = page->next;
    }

    *out_count = list->total_tokens;
    return buf;
}

// ── Helpers ─────────────────────────────────────────────────────────────────

static void uint_to_str(char* buf, size_t buf_size, uint64_t value, size_t* out_len) {
    if (buf_size == 0) return;
    if (value == 0) { buf[0] = '0'; buf[1] = '\0'; *out_len = 1; return; }
    char temp[21];
    size_t len = 0;
    while (value > 0 && len < sizeof(temp) - 1) {
        temp[len++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    size_t out_len_val = (len < buf_size) ? len : buf_size - 1;
    for (size_t i = 0; i < out_len_val; ++i)
        buf[i] = temp[len - 1 - i];
    buf[out_len_val] = '\0';
    *out_len = out_len_val;
}

static const char* make_error_msg(KalidousArena* arena, const char* prefix, uint64_t line) {
    char stack_buf[128];
    size_t pos = 0;
    for (const char* p = prefix; *p && pos < sizeof(stack_buf) - 22;)
        stack_buf[pos++] = *p++;

    size_t num_len = 0;
    uint_to_str(stack_buf + pos, sizeof(stack_buf) - pos, line, &num_len);
    pos += num_len;

    char* msg = static_cast<char*>(kalidous_arena_alloc(arena, pos + 1));
    if (msg) std::memcpy(msg, stack_buf, pos + 1);
    return msg;
}

static KalidousToken make_token(KalidousArena* arena, KalidousTokenType type,
                            std::string_view lexeme, KalidousSourceLoc info) {
    auto* buf = static_cast<char*>(kalidous_arena_alloc(arena, lexeme.size()));
    if (buf && !lexeme.empty())
        std::memcpy(buf, lexeme.data(), lexeme.size());
    return KalidousToken{
        .lexeme     = {buf, lexeme.size()},
        .loc        = info,
        .type       = type,
        .keyword_id = 0
    };
}

// ── Char classifiers ─────────────────────────────────────────────────────────

static bool isSpace   (unsigned char c) { return ::isspace(c)  != 0; }
static bool isAlpha   (unsigned char c) { return ::isalpha(c)  != 0; }
static bool isDigit   (unsigned char c) { return ::isdigit(c)  != 0; }
static bool isAlphaNum(unsigned char c) { return ::isalnum(c)  != 0; }
static bool isHexDigit(unsigned char c) { return ::isxdigit(c) != 0; }
static unsigned char toLower(unsigned char c) { return static_cast<unsigned char>(::tolower(c)); }

static bool isValidEscape(char c) {
    switch (c) {
        case 'n': case 't': case 'r': case '\\': case '"': case '0': case '\'':
            return true;
        default:
            return false;
    }
}

// ── Forward declarations ─────────────────────────────────────────────────────

static void processIdentifier(const char*& current, const char* end,
                              TokenList& tokens,
                              KalidousSourceLoc& info, KalidousArena* arena);

static void processString(const char*& current, const char* end,
                          TokenList& tokens, std::vector<LexError>& error_list,
                          KalidousSourceLoc& info, KalidousArena* arena);

static void processNumber(const char*& current, const char* end,
                          TokenList& tokens, std::vector<LexError>& error_list,
                          KalidousSourceLoc& info, KalidousArena* arena);

static bool punctuation(const char*& current, const char* end,
                        TokenList& tokens,
                        KalidousSourceLoc& info, KalidousArena* arena);

// ── Comments ─────────────────────────────────────────────────────────────────

static void skipSingleLine(KalidousSourceLoc& info, const char*& current, const char* end) {
    while (current < end && *current != '\n') {
        ++current; ++info.index;
    }
}

static void skipMultiLine(KalidousSourceLoc& info, const char*& current, const char* end,
                          std::vector<LexError>& error_list, KalidousArena* arena) {
    const auto start = info;
    current += 2; info.index += 2;

    while (current < end) {
        if (*current == '*' && current + 1 < end && *(current + 1) == '/') {
            current += 2; info.index += 2;
            return;
        }
        if (*current == '\n') KALIDOUS_NEWLINE(&info);
        ++current; ++info.index;
    }
    error_list.push_back({
        make_error_msg(arena, "Unterminated multi-line comment starting at line ", start.line),
        start
    });
}

// ── Errors ───────────────────────────────────────────────────────────────────

static void addCharError(std::vector<LexError>& error_list, const char* base_msg,
                         const char c, const KalidousSourceLoc info, KalidousArena* arena) {
    if (error_list.size() >= MAX_ERRORS) return;

    char stack_buf[64];
    size_t pos = 0;
    for (const char* p = base_msg; *p && pos < sizeof(stack_buf) - 5;)
        stack_buf[pos++] = *p++;
    stack_buf[pos++] = '\'';
    stack_buf[pos++] = c;
    stack_buf[pos++] = '\'';
    stack_buf[pos]   = '\0';
    error_list.push_back({ make_error_msg(arena, stack_buf, info.line), info });
}

static void addMsgError(std::vector<LexError>& error_list, KalidousArena* arena,
                        const char* msg, const KalidousSourceLoc info) {
    if (error_list.size() >= MAX_ERRORS) return;

    const size_t len = strlen(msg); // Only used for error messages, rare path
    char* copy = static_cast<char*>(kalidous_arena_alloc(arena, len + 1));
    if (copy) std::memcpy(copy, msg, len + 1);
    error_list.push_back({ copy, info });
}

// ── Processors ──────────────────────────────────────────────────────────────

static void processIdentifier(const char*& current, const char* end,
                              TokenList& tokens,
                              KalidousSourceLoc& info, KalidousArena* arena) {
    const KalidousSourceLoc startInfo = info;
    const char* start = current;

    while (current < end && (isAlphaNum(static_cast<unsigned char>(*current)) || *current == '_')) {
        ++current; ++info.index;
    }
    const std::string_view lexeme(start, current - start);
    const KalidousTokenType type = kalidous_lookup_keyword(start, current - start);
    token_list_push(&tokens, arena, make_token(arena, type, lexeme, startInfo));
}

static void processString(const char*& current, const char* end,
                          TokenList& tokens, std::vector<LexError>& error_list,
                          KalidousSourceLoc& info, KalidousArena* arena) {
    const KalidousSourceLoc startInfo = info;
    const char* start = current;
    ++current; ++info.index;

    while (current < end) {
        if (*current == '"') {
            ++current; ++info.index;
            token_list_push(&tokens, arena, make_token(arena, KALIDOUS_TOKEN_STRING,
                                        std::string_view(start, current - start), startInfo));
            return;
        }

        if (*current == '\\') {
            const KalidousSourceLoc escapeLoc = info;
            ++current; ++info.index;

            if (current >= end) {
                addMsgError(error_list, arena,
                    "Unterminated escape sequence at end of file", escapeLoc);
                goto terminate_string;
            }

            if (!isValidEscape(*current)) {
                char msg_buf[64];
                size_t pos = 0;
                const char* pfx = "Invalid escape sequence '\\";
                for (const char* p = pfx; *p && pos < sizeof(msg_buf) - 2;) msg_buf[pos++] = *p++;
                if (pos < sizeof(msg_buf) - 1) msg_buf[pos++] = *current;
                if (pos < sizeof(msg_buf) - 1) msg_buf[pos++] = '\'';
                msg_buf[pos] = '\0';

                error_list.push_back({
                    make_error_msg(arena, msg_buf, escapeLoc.line),
                    escapeLoc
                });
            }
            ++current; ++info.index;
            continue;
        }

        if (*current == '\n') KALIDOUS_NEWLINE(&info);
        ++current; ++info.index;
    }

terminate_string:
    error_list.push_back({
        make_error_msg(arena, "Unterminated string literal starting at line ", startInfo.line),
        startInfo
    });
    token_list_push(&tokens, arena, make_token(arena, KALIDOUS_TOKEN_STRING,
                                std::string_view(start, current - start), startInfo));
}

static void processNumber(const char*& current, const char* end,
                          TokenList& tokens, std::vector<LexError>& error_list,
                          KalidousSourceLoc& info, KalidousArena* arena) {
    const char*           start     = current;
    const KalidousSourceLoc   startInfo = info;

    enum class Base { Decimal, Hex, Binary, Octal } base = Base::Decimal;

    // Detect Base Prefix
    if (*current == '0' && current + 1 < end) {
        const char next = static_cast<char>(toLower(static_cast<unsigned char>(*(current + 1))));
        if (next == 'x') {
            base = Base::Hex;
            current += 2; info.index += 2;
            if (current >= end || !isHexDigit(static_cast<unsigned char>(*current))) {
                error_list.push_back({
                    make_error_msg(arena, "Hex literal '0x' has no digits at line ", startInfo.line),
                    startInfo
                });
                token_list_push(&tokens, arena, make_token(arena, KALIDOUS_TOKEN_HEXADECIMAL,
                                            std::string_view(start, current - start), startInfo));
                return;
            }
        } else if (next == 'b') {
            base = Base::Binary;
            current += 2; info.index += 2;
            if (current >= end || (*current != '0' && *current != '1')) {
                error_list.push_back({
                    make_error_msg(arena, "Binary literal '0b' has no digits at line ", startInfo.line),
                    startInfo
                });
                token_list_push(&tokens, arena, make_token(arena, KALIDOUS_TOKEN_BINARY,
                                            std::string_view(start, current - start), startInfo));
                return;
            }
        } else if (next == 'o') {
            base = Base::Octal;
            current += 2; info.index += 2;
            if (current >= end || *current < '0' || *current > '7') {
                error_list.push_back({
                    make_error_msg(arena, "Octal literal '0o' has no digits at line ", startInfo.line),
                    startInfo
                });
                token_list_push(&tokens, arena, make_token(arena, KALIDOUS_TOKEN_OCTAL,
                                            std::string_view(start, current - start), startInfo));
                return;
            }
        }
    }

    bool isFloat = false;
    bool prev_is_separator = false; // Track separators
    bool has_digit = false;

    while (current < end) {
        const auto c = static_cast<unsigned char>(*current);

        if (c == '\'') { // Changed from _ to '
            if (prev_is_separator) {
                error_list.push_back({
                    make_error_msg(arena, "Consecutive separators in numeric literal at line ", info.line),
                    info
                });
            }
            prev_is_separator = true;
            ++current; ++info.index;
            continue;
        }

        switch (base) {
            case Base::Hex:
                if (!isHexDigit(c)) goto done;
                break;
            case Base::Binary:
                if (c != '0' && c != '1') goto done;
                break;
            case Base::Octal:
                if (c >= '0' && c <= '7') {
                    if (current + 1 < end) {
                        const unsigned char next = static_cast<unsigned char>(*(current + 1));
                        if (next == '8' || next == '9') {
                            KalidousSourceLoc errLoc = info;
                            errLoc.index += 1;
                            ++current; ++info.index;
                            error_list.push_back({
                                make_error_msg(arena, "Invalid digit '8' or '9' in octal literal at line ", errLoc.line),
                                errLoc
                            });
                            ++current; ++info.index;
                            goto done;
                        }
                    }
                } else {
                    goto done;
                }
                break;
            case Base::Decimal:
                if (c == '.') {
                    if (isFloat) goto done;
                    if (!(current + 1 < end && isDigit(static_cast<unsigned char>(*(current + 1)))))
                        goto done;
                    isFloat = true;
                } else if (!isDigit(c)) {
                    goto done;
                }
                break;
        }

        if (c != '\'') has_digit = true;
        prev_is_separator = false;
        ++current; ++info.index;
    }

done:
    if (prev_is_separator) {
        error_list.push_back({
            make_error_msg(arena, "Trailing separator in numeric literal at line ", info.line),
            info
        });
    }

    if (current < end && (isAlpha(static_cast<unsigned char>(*current)) || *current == '_')) {
        const char* suffixStart = current;
        const KalidousSourceLoc suffixLoc = info;

        while (current < end && (isAlphaNum(static_cast<unsigned char>(*current)) || *current == '_')) {
            ++current; ++info.index;
        }

        char msg_buf[160];
        size_t pos = 0;
        const char* pfx = "Invalid suffix '";
        for (const char* p = pfx; *p && pos < sizeof(msg_buf) - 1;) msg_buf[pos++] = *p++;
        for (const char* p = suffixStart; p < current && pos < sizeof(msg_buf) - 1;) msg_buf[pos++] = *p++;
        const char* sfx = "' on numeric literal at line ";
        for (const char* p = sfx; *p && pos < sizeof(msg_buf) - 1;) msg_buf[pos++] = *p++;
        msg_buf[pos] = '\0';

        error_list.push_back({
            make_error_msg(arena, msg_buf, suffixLoc.line),
            suffixLoc
        });
    }

    KalidousTokenType type = KALIDOUS_TOKEN_NUMBER;
    switch (base) {
        case Base::Hex:     type = KALIDOUS_TOKEN_HEXADECIMAL; break;
        case Base::Binary:  type = KALIDOUS_TOKEN_BINARY;      break;
        case Base::Octal:   type = KALIDOUS_TOKEN_OCTAL;       break;
        case Base::Decimal: type = isFloat ? KALIDOUS_TOKEN_FLOAT : KALIDOUS_TOKEN_NUMBER; break;
    }

    token_list_push(&tokens, arena, make_token(arena, type,
                                std::string_view(start, current - start), startInfo));
}

static bool punctuation(const char*& current, const char* end,
                        TokenList& tokens,
                        KalidousSourceLoc& info, KalidousArena* arena) {
    const char c = *current;

    switch (c) {
        // Optimized single-char operators
        case '(': case ')': case '{': case '}': case '[': case ']':
        case ';': case ',': case ':': case '?': case '@': case '#':
        case '~':
            token_list_push(&tokens, arena, make_token(arena, kalidous_lookup_keyword(&c, 1),
                                        std::string_view(&c, 1), info));
            ++current; ++info.index;
            return true;

        // Potential multi-char operators
        case '+': case '-': case '*': case '/': case '%': case '^':
        case '&': case '|': case '=': case '!': case '<': case '>':
        case '.':
            break;
        default:
            return false;
    }

    // Multi-character check
    for (const int len : {3, 2, 1}) {
        if (current + len > end) continue;
        const auto t = kalidous_lookup_keyword(current, static_cast<size_t>(len));
        if (t != KALIDOUS_TOKEN_IDENTIFIER) {
            const std::string_view view(current, static_cast<size_t>(len));
            current    += len;
            info.index += static_cast<size_t>(len);
            token_list_push(&tokens, arena, make_token(arena, t, view, info));
            return true;
        }
    }
    return false;
}

// ── Main Loop ───────────────────────────────────────────────────────────────

static void tokenize(std::string_view src, KalidousArena* arena,
                     TokenList& tokens, std::vector<LexError>& error_list) {

    token_list_init(&tokens, arena, 64); // Start with space for 64 tokens per page

    KalidousSourceLoc info{0, 1};
    const char* current = src.data();
    const char* end     = src.data() + src.size();

    while (current < end) {
        const auto c = static_cast<unsigned char>(*current);

        if (isSpace(c)) {
            if (*current == '\n') KALIDOUS_NEWLINE(&info);
            ++current; ++info.index;
            continue;
        }

        if (*current == '/' && current + 1 < end) {
            if (*(current + 1) == '/') { skipSingleLine(info, current, end); continue; }
            if (*(current + 1) == '*') { skipMultiLine(info, current, end, error_list, arena); continue; }
        }

        if (isAlpha(c) || *current == '_') {
            processIdentifier(current, end, tokens, info, arena);
            continue;
        }

        if (isDigit(c) || (*current == '.' && current + 1 < end &&
                            isDigit(static_cast<unsigned char>(*(current + 1))))) {
            processNumber(current, end, tokens, error_list, info, arena);
            continue;
        }

        if (*current == '"') {
            processString(current, end, tokens, error_list, info, arena);
            continue;
        }

        if (punctuation(current, end, tokens, info, arena))
            continue;

        addCharError(error_list, "Unknown character ", *current, info, arena);
        token_list_push(&tokens, arena, make_token(arena, KALIDOUS_TOKEN_UNKNOWN,
                                    std::string_view(current, 1), info));
        ++current; ++info.index;

        if (error_list.size() >= MAX_ERRORS) break;
    }

    token_list_push(&tokens, arena, make_token(arena, KALIDOUS_TOKEN_END, std::string_view{}, info));
}

} // namespace kalidous::detail

// ── C API ─────────────────────────────────────────────────────────────────────

KalidousTokenStream kalidous_tokenize(KalidousArena* arena, const char* source, const size_t source_len) {
    if (!arena || !source) return {nullptr, 0};

    std::vector<kalidous::detail::LexError> error_list;
    kalidous::detail::TokenList tokens{};

    kalidous::detail::tokenize(std::string_view(source, source_len), arena, tokens, error_list);

    if (!error_list.empty()) {
        for (const auto&[msg, info] : error_list)
            std::cerr << "Lexical error (line " << info.line
                      << ", col " << info.index << "): " << msg << '\n';
        return {nullptr, 0};
    }

    size_t count = 0;
    KalidousToken* flat_data = kalidous::detail::token_list_flatten(&tokens, arena, &count);

    return {flat_data, count};
}

#undef KALIDOUS_NEWLINE