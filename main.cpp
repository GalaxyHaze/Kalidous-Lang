// main.cpp
#include <iostream>
#include <cstdlib>
#include "Nova/memory/arena_c_functions.h"
#include "Nova/utils/i_o_utils.h"        // C API: nova_load_file_to_arena
#include "Nova/parse/tokenizer.h"   // C API: nova_tokenize
#include "Nova/parse/tokens.h"      // NovaToken, NovaTokenType

// Helper: print token type as string (for debugging)
static const char* token_type_name(NovaTokenType t) {
    switch (t) {
#define CASE(x) case NOVA_TOKEN_##x: return #x;
        CASE(STRING)
        CASE(NUMBER)
        CASE(TYPE)
        CASE(IDENTIFIER)
        CASE(MODIFIER)
        CASE(ASSIGNMENT)
        CASE(EQUAL)
        CASE(NOT_EQUAL)
        CASE(PLUS)
        CASE(MINUS)
        CASE(MULTIPLY)
        CASE(DIVIDE)
        CASE(CONST)
        CASE(LET)
        CASE(AUTO)
        CASE(MUTABLE)
        CASE(GREATER_THAN)
        CASE(LESS_THAN)
        CASE(GREATER_THAN_OR_EQUAL)
        CASE(LESS_THAN_OR_EQUAL)
        CASE(AND)
        CASE(OR)
        CASE(LPAREN)
        CASE(RPAREN)
        CASE(LBRACE)
        CASE(RBRACE)
        CASE(LBRACKET)
        CASE(RBRACKET)
        CASE(COMMA)
        CASE(COLON)
        CASE(SEMICOLON)
        CASE(UNKNOWN)
        CASE(RETURN)
        CASE(END)
        CASE(IF)
        CASE(ELSE)
        CASE(WHILE)
        CASE(FOR)
        CASE(IN)
        CASE(ARROW)
        CASE(PLUS_EQUAL)
        CASE(MINUS_EQUAL)
        CASE(MULTIPLY_EQUAL)
        CASE(DIVIDE_EQUAL)
        CASE(DOT)
        CASE(DOTS)
        CASE(SWITCH)
        CASE(STRUCT)
        CASE(ENUM)
        CASE(UNION)
        CASE(FAMILY)
        CASE(BREAK)
        CASE(CONTINUE)
        CASE(MOD)
        CASE(ENTITY)
        CASE(FLOAT)
        CASE(NOT)
        CASE(HEXADECIMAL)
        CASE(OCTONAL)
        CASE(BINARY)
#undef CASE
        default: return "???";
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file.nova>\n";
        return 1;
    }

    const char* filepath = argv[1];

    // 1. Create Arena
    NovaArena* arena = nova_arena_create(64 * 1024); // 64 KiB initial block
    if (!arena) {
        std::cerr << "Failed to create arena\n";
        return 1;
    }

    // 2. Load file into Arena
    size_t file_size = 0;
    char* source = nova_load_file_to_arena(arena, filepath, &file_size);
    if (!source) {
        std::cerr << "Failed to load file: " << filepath << "\n";
        nova_arena_destroy(arena);
        return 1;
    }

    // Ensure null-termination for safety (optional, but nice for debug prints)
    // Note: tokenizer doesn't require it — it uses length
    if (file_size == 0) {
        // Handle empty file
        source = static_cast<char*>(nova_arena_alloc(arena, 1));
        source[0] = '\0';
        file_size = 0;
    }

    // 3. Tokenize
    NovaTokenSlice tokens = nova_tokenize(arena, source, file_size);
    if (!tokens.data) {
        std::cerr << "Tokenization failed\n";
        nova_arena_destroy(arena);
        return 1;
    }

    // 4. Print tokens
    std::cout << "=== Tokens ===\n";
    for (size_t i = 0; i < tokens.len; ++i) {
        const NovaToken& tok = tokens.data[i];
        std::string_view lexeme(tok.value, tok.value_len);

        // Print line:col | TYPE | "lexeme"
        std::cout << tok.info.line << ":" << tok.info.index
                  << " | " << token_type_name(tok.token)
                  << " | \"" << lexeme << "\"\n";

        if (tok.token == NOVA_TOKEN_END) break;
    }

    // 5. Clean up (Arena owns everything)
    nova_arena_destroy(arena);
    return 0;
}