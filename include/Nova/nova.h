// nova.h - Unified header for Nova programming language core
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Core Types & Utilities
// ============================================================================

// Source location information
typedef struct {
    size_t index;  // 0-based column/offset in current line
    size_t line;   // 1-based line number
} NovaSourceLoc;

// Generic slice (view into contiguous memory)
typedef struct {
    const void* data;
    size_t len;
} NovaSlice;

// String slice (non-null-terminated)
typedef struct {
    const char* data;
    size_t len;
} NovaStr;

// ============================================================================
// Token System
// ============================================================================

    typedef enum {
        NOVA_TOKEN_STRING,
        NOVA_TOKEN_NUMBER,
        NOVA_TOKEN_TYPE,
        NOVA_TOKEN_IDENTIFIER,
        NOVA_TOKEN_MODIFIER,

        NOVA_TOKEN_ASSIGNMENT,
        NOVA_TOKEN_EQUAL,
        NOVA_TOKEN_NOT_EQUAL,
        NOVA_TOKEN_PLUS,
        NOVA_TOKEN_MINUS,
        NOVA_TOKEN_MULTIPLY,
        NOVA_TOKEN_DIVIDE,

        NOVA_TOKEN_CONST,
        NOVA_TOKEN_LET,
        NOVA_TOKEN_AUTO,
        NOVA_TOKEN_MUTABLE,

        NOVA_TOKEN_GREATER_THAN,
        NOVA_TOKEN_LESS_THAN,
        NOVA_TOKEN_GREATER_THAN_OR_EQUAL,
        NOVA_TOKEN_LESS_THAN_OR_EQUAL,

        NOVA_TOKEN_AND,
        NOVA_TOKEN_OR,

        NOVA_TOKEN_LPAREN,
        NOVA_TOKEN_RPAREN,
        NOVA_TOKEN_LBRACE,
        NOVA_TOKEN_RBRACE,
        NOVA_TOKEN_LBRACKET,
        NOVA_TOKEN_RBRACKET,

        NOVA_TOKEN_COMMA,
        NOVA_TOKEN_COLON,
        NOVA_TOKEN_SEMICOLON,

        NOVA_TOKEN_UNKNOWN,

        NOVA_TOKEN_RETURN,
        NOVA_TOKEN_END,
        NOVA_TOKEN_IF,
        NOVA_TOKEN_ELSE,
        NOVA_TOKEN_WHILE,
        NOVA_TOKEN_FOR,
        NOVA_TOKEN_IN,
        NOVA_TOKEN_ARROW,
        NOVA_TOKEN_PLUS_EQUAL,
        NOVA_TOKEN_MINUS_EQUAL,
        NOVA_TOKEN_MULTIPLY_EQUAL,
        NOVA_TOKEN_DIVIDE_EQUAL,
        NOVA_TOKEN_DOT,
        NOVA_TOKEN_DOTS,
        NOVA_TOKEN_SWITCH,
        NOVA_TOKEN_STRUCT,
        NOVA_TOKEN_ENUM,
        NOVA_TOKEN_UNION,
        NOVA_TOKEN_FAMILY,
        NOVA_TOKEN_BREAK,
        NOVA_TOKEN_CONTINUE,
        NOVA_TOKEN_MOD,
        NOVA_TOKEN_ENTITY,
        NOVA_TOKEN_FLOAT,
        NOVA_TOKEN_NOT,
        NOVA_TOKEN_HEXADECIMAL,
        NOVA_TOKEN_OCTONAL,
        NOVA_TOKEN_BINARY
    } NovaTokenType;

// Token with source location
typedef struct {
    NovaStr lexeme;        // Points into source buffer
    NovaSourceLoc loc;
    NovaTokenType type;
    uint16_t keyword_id;   // For NOVA_TOKEN_KEYWORD: 0=if, 1=else, etc.
} NovaToken;

// Token stream slice
typedef struct {
    const NovaToken* data;
    size_t len;
} NovaTokenStream;

// Opaque arena handle
typedef struct NovaArena NovaArena;

// Tokenize source (allocates tokens in arena)
NovaTokenStream nova_tokenize(NovaArena* arena, const char* source, size_t source_len);

// ============================================================================
// AST System (Generic & Extensible)
// ============================================================================

// Generic node type identifier - allows unlimited extensibility
typedef uint16_t NovaNodeId;

// Core node types (minimal set that covers 95% of languages)
enum {
    NOVA_NODE_ERROR = 0,        // Invalid/errored node

    // Expressions
    NOVA_NODE_LITERAL = 100,    // Number, string, boolean, null
    NOVA_NODE_IDENTIFIER = 101,
    NOVA_NODE_BINARY_OP = 102,  // +, -, *, /, ==, etc.
    NOVA_NODE_UNARY_OP = 103,   // !, -, ++, --
    NOVA_NODE_CALL = 104,       // function(args)
    NOVA_NODE_INDEX = 105,      // array[index]
    NOVA_NODE_MEMBER = 106,     // obj.field

    // Declarations
    NOVA_NODE_VAR_DECL = 200,   // let/const/var x = expr
    NOVA_NODE_FUNC_DECL = 201,  // fn name(params) -> ret { body }
    NOVA_NODE_PARAM = 202,      // function parameter

    // Statements
    NOVA_NODE_BLOCK = 300,      // { stmts... }
    NOVA_NODE_IF = 301,         // if cond { then } else { alt }
    NOVA_NODE_WHILE = 302,      // while cond { body }
    NOVA_NODE_RETURN = 303,     // return expr;
    NOVA_NODE_EXPR_STMT = 304,  // expr; (expression as statement)

    // Types (optional - for typed variants)
    NOVA_NODE_TYPE_REF = 400,   // Type name reference
    NOVA_NODE_TYPE_FUNC = 401,  // (A, B) -> C

    // Custom/extensible nodes start at 1000+
    NOVA_NODE_CUSTOM_START = 1000
};

// Generic AST node (tagged union pattern)
typedef struct NovaNode NovaNode;
struct NovaNode {
    NovaNodeId type;      // Node type identifier
    NovaSourceLoc loc;    // Source location of this node

    // Generic child storage (interpretation depends on node type)
    union {
        struct { NovaNode* a; NovaNode* b; NovaNode* c; } kids;  // Up to 3 children
        struct { void* ptr; size_t len; } list;                  // Child list
        struct { const char* str; size_t len; } ident;           // Identifier/text
        struct { double num; } number;                           // Numeric literal
        struct { bool value; } boolean;                          // Boolean literal
        uint64_t custom;  // Custom payload for extension nodes
    } data;
};


// Parse token stream into AST (allocates nodes in arena)
NovaNode* nova_parse(NovaArena* arena, NovaTokenStream tokens);

// Get node type safely
static inline NovaNodeId nova_node_type(const NovaNode* node) {
    return node ? node->type : NOVA_NODE_ERROR;
}

// ============================================================================
// Memory Arena (Bump Allocator)
// ============================================================================



NovaArena* nova_arena_create(size_t initial_block_size);
void*      nova_arena_alloc(NovaArena* arena, size_t size);
char*      nova_arena_strdup(NovaArena* arena, const char* str);
void       nova_arena_reset(NovaArena* arena);
void       nova_arena_destroy(NovaArena* arena);

// ============================================================================
// File Utilities (C-compatible)
// ============================================================================

bool  nova_file_exists(const char* path);
bool  nova_file_is_regular(const char* path);
size_t nova_file_size(const char* path);
bool  nova_file_has_extension(const char* path, const char* ext); // case-insensitive

// Load entire file into arena (returns NULL on error, sets *out_size)
char* nova_load_file_to_arena(NovaArena* arena, const char* path, size_t* out_size);

int nova_run(const int argc, const char** argv);
NovaTokenType nova_lookup_keyword(const char* str, size_t len);

// ============================================================================
// Error Handling
// ============================================================================

typedef enum {
    NOVA_OK = 0,
    NOVA_ERR_IO,
    NOVA_ERR_PARSE,
    NOVA_ERR_LEX,
    NOVA_ERR_MEMORY,
    NOVA_ERR_INVALID_INPUT
} NovaError;

// ============================================================================
// C++ Extensions (optional)
// ============================================================================

#ifdef __cplusplus
}
#include <memory>
#include <string_view>
#include <string.h>

namespace nova {

// RAII wrapper for arena
class Arena {

    struct Deleter { void operator()(NovaArena* a) const { nova_arena_destroy(a); } };
    std::unique_ptr<NovaArena, Deleter> handle_;

public:
    explicit Arena(size_t initial = 65536)
        : handle_(nova_arena_create(initial)) { if (!handle_) throw std::bad_alloc(); }

    explicit Arena(NovaArena* src): handle_(src) {}

    void* alloc(size_t size) const { return nova_arena_alloc(handle_.get(), size); }

    char* strdup(const char* s) const { return nova_arena_strdup(handle_.get(), s); }

    char* strdup(std::string_view sv) const {
        char* p = static_cast<char*>(alloc(sv.size() + 1));
        if (p) { memcpy(p, sv.data(), sv.size()); p[sv.size()] = '\0'; }
        return p;
    }

    NovaArena* get() const { return handle_.get(); }
};

// Modern tokenizer wrapper
inline NovaTokenStream tokenize(Arena& arena, std::string_view source) {
    return nova_tokenize(arena.get(), source.data(), source.size());
}

// File loader with exception safety
inline std::pair<char*, size_t> load_file(Arena& arena, const char* path) {
    size_t size = 0;
    char* data = nova_load_file_to_arena(arena.get(), path, &size);
    if (!data) throw std::runtime_error("Failed to load file: " + std::string(path));
    return {data, size};
}

// Debug utilities (optional - not in core API)
namespace debug {
    const char* token_type_name(NovaTokenType t);
    void print_tokens(NovaTokenStream tokens);
    void print_ast(const NovaNode* node, int indent = 0);
}

} // namespace nova
#endif // __cplusplus