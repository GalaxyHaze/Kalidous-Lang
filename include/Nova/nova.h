// nova.h - Unified header for Nova programming language core
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Core Types & Utilities
// ============================================================================

typedef struct {
    size_t index;
    size_t line;
} NovaSourceLoc;

typedef struct {
    const void* data;
    size_t len;
} NovaSlice;

typedef struct {
    const char* data;
    size_t len;
} NovaStr;

// ============================================================================
// Token System
// ============================================================================

typedef enum {
    // ------------------------------------------------------------------------
    // Literais e identificadores
    // ------------------------------------------------------------------------
    NOVA_TOKEN_STRING,
    NOVA_TOKEN_NUMBER,
    NOVA_TOKEN_HEXADECIMAL,
    NOVA_TOKEN_OCTAL,
    NOVA_TOKEN_BINARY,
    NOVA_TOKEN_FLOAT,
    NOVA_TOKEN_IDENTIFIER,

    // ------------------------------------------------------------------------
    // Operadores aritméticos e lógicos
    // ------------------------------------------------------------------------
    NOVA_TOKEN_PLUS,
    NOVA_TOKEN_MINUS,
    NOVA_TOKEN_MULTIPLY,
    NOVA_TOKEN_DIVIDE,
    NOVA_TOKEN_MOD,

    NOVA_TOKEN_AND,
    NOVA_TOKEN_OR,
    NOVA_TOKEN_NOT,

    // ------------------------------------------------------------------------
    // Operadores de comparação
    // ------------------------------------------------------------------------
    NOVA_TOKEN_EQUAL,
    NOVA_TOKEN_NOT_EQUAL,
    NOVA_TOKEN_LESS_THAN,
    NOVA_TOKEN_GREATER_THAN,
    NOVA_TOKEN_LESS_THAN_OR_EQUAL,
    NOVA_TOKEN_GREATER_THAN_OR_EQUAL,

    // ------------------------------------------------------------------------
    // Operadores de atribuição
    // ------------------------------------------------------------------------
    NOVA_TOKEN_ASSIGNMENT,
    NOVA_TOKEN_DECLARATION,     // :=
    NOVA_TOKEN_PLUS_EQUAL,
    NOVA_TOKEN_MINUS_EQUAL,
    NOVA_TOKEN_MULTIPLY_EQUAL,
    NOVA_TOKEN_DIVIDE_EQUAL,

    // ------------------------------------------------------------------------
    // Operadores especiais
    // ------------------------------------------------------------------------
    NOVA_TOKEN_QUESTION,        // ?  optional
    NOVA_TOKEN_BANG,            // !  type may fail
    NOVA_TOKEN_ARROW,           // -> encadeamento de funções

    // ------------------------------------------------------------------------
    // Delimitadores
    // ------------------------------------------------------------------------
    NOVA_TOKEN_LPAREN,
    NOVA_TOKEN_RPAREN,
    NOVA_TOKEN_LBRACE,
    NOVA_TOKEN_RBRACE,
    NOVA_TOKEN_LBRACKET,
    NOVA_TOKEN_RBRACKET,
    NOVA_TOKEN_DOT,
    NOVA_TOKEN_DOTS,            // ...
    NOVA_TOKEN_COMMA,
    NOVA_TOKEN_COLON,
    NOVA_TOKEN_SEMICOLON,

    // ------------------------------------------------------------------------
    // Keywords: controle de fluxo
    // ------------------------------------------------------------------------
    NOVA_TOKEN_IF,
    NOVA_TOKEN_ELSE,
    NOVA_TOKEN_FOR,
    NOVA_TOKEN_IN,
    NOVA_TOKEN_WHILE,           // reservado na ABI, não usado como keyword ativa
    NOVA_TOKEN_SWITCH,
    NOVA_TOKEN_RETURN,
    NOVA_TOKEN_BREAK,
    NOVA_TOKEN_CONTINUE,
    NOVA_TOKEN_GOTO,
    NOVA_TOKEN_MARKER,
    NOVA_TOKEN_SCENE,

    // ------------------------------------------------------------------------
    // Keywords: concorrência / fluxo assíncrono
    // ------------------------------------------------------------------------
    NOVA_TOKEN_SPAWN,
    NOVA_TOKEN_JOINED,
    NOVA_TOKEN_AWAIT,

    // ------------------------------------------------------------------------
    // Keywords: tratamento de erros
    // ------------------------------------------------------------------------
    NOVA_TOKEN_TRY,
    NOVA_TOKEN_CATCH,
    NOVA_TOKEN_MUST,            // "must!" — o ! é semântico, o Parser resolve

    // ------------------------------------------------------------------------
    // Modificadores de propriedade e escopo
    // ------------------------------------------------------------------------
    NOVA_TOKEN_CONST,
    NOVA_TOKEN_MUTABLE,         // keyword: 'mut'
    NOVA_TOKEN_VAR,
    NOVA_TOKEN_LET,
    NOVA_TOKEN_AUTO,

    NOVA_TOKEN_GLOBAL,
    NOVA_TOKEN_PERSISTENT,
    NOVA_TOKEN_LOCAL,
    NOVA_TOKEN_LEND,
    NOVA_TOKEN_SHARED,
    NOVA_TOKEN_VIEW,
    NOVA_TOKEN_UNIQUE,
    NOVA_TOKEN_PACK,            // reservado na ABI; [] é resolvido pelo Parser

    // ------------------------------------------------------------------------
    // Modificadores de acesso
    // ------------------------------------------------------------------------
    NOVA_TOKEN_MODIFIER,        // public / private / protected

    // ------------------------------------------------------------------------
    // Declarações de tipo
    // ------------------------------------------------------------------------
    NOVA_TOKEN_TYPE,
    NOVA_TOKEN_STRUCT,
    NOVA_TOKEN_COMPONENT,
    NOVA_TOKEN_ENUM,
    NOVA_TOKEN_UNION,
    NOVA_TOKEN_FAMILY,
    NOVA_TOKEN_ENTITY,
    NOVA_TOKEN_TRAIT,
    NOVA_TOKEN_TYPEDEF,
    NOVA_TOKEN_IMPLEMENT,

    // ------------------------------------------------------------------------
    // Tokens especiais / controle
    // ------------------------------------------------------------------------
    NOVA_TOKEN_END,
    NOVA_TOKEN_UNKNOWN
} NovaTokenType;

typedef struct {
    NovaStr lexeme;
    NovaSourceLoc loc;
    NovaTokenType type;
    uint16_t keyword_id;
} NovaToken;

typedef struct {
    const NovaToken* data;
    size_t len;
} NovaTokenStream;

typedef struct NovaArena NovaArena;

NovaTokenNovaTokenStream nova_tokenize(NovaArena* arena, const char* source, size_t source_len);

// ============================================================================
// AST System
// ============================================================================

typedef uint16_t NovaNodeId;

enum {
    NOVA_NODE_ERROR = 0,

    NOVA_NODE_LITERAL    = 100,
    NOVA_NODE_IDENTIFIER = 101,
    NOVA_NODE_BINARY_OP  = 102,
    NOVA_NODE_UNARY_OP   = 103,
    NOVA_NODE_CALL       = 104,
    NOVA_NODE_INDEX      = 105,
    NOVA_NODE_MEMBER     = 106,

    NOVA_NODE_VAR_DECL   = 200,
    NOVA_NODE_FUNC_DECL  = 201,
    NOVA_NODE_PARAM      = 202,

    NOVA_NODE_BLOCK      = 300,
    NOVA_NODE_IF         = 301,
    NOVA_NODE_FOR        = 302,  // unifica for e while
    NOVA_NODE_RETURN     = 303,
    NOVA_NODE_EXPR_STMT  = 304,

    NOVA_NODE_TYPE_REF   = 400,
    NOVA_NODE_TYPE_FUNC  = 401,

    NOVA_NODE_CUSTOM_START = 1000
};

typedef struct NovaNode NovaNode;
struct NovaNode {
    NovaNodeId type;
    NovaSourceLoc loc;

    union {
        struct { NovaNode* a; NovaNode* b; NovaNode* c; } kids;
        struct { void* ptr; size_t len; } list;
        struct { const char* str; size_t len; } ident;
        struct { double num; } number;
        struct { bool value; } boolean;
        uint64_t custom;
    } data;
};

NovaNode* nova_parse(NovaArena* arena, NovaTokenStream tokens);
static inline NovaNodeId nova_node_type(const NovaNode* node) {
        return node ? node->type : (NovaNodeId)NOVA_NODE_ERROR;
}

// ============================================================================
// Memory Arena
// ============================================================================

NovaArena* nova_arena_create(size_t initial_block_size);
void*      nova_arena_alloc(NovaArena* arena, size_t size);
char*      nova_arena_strdup(NovaArena* arena, const char* str);
void       nova_arena_reset(NovaArena* arena);
void       nova_arena_destroy(NovaArena* arena);

// ============================================================================
// File Utilities
// ============================================================================

bool   nova_file_exists(const char* path);
bool   nova_file_is_regular(const char* path);
size_t nova_file_size(const char* path);
bool   nova_file_has_extension(const char* path, const char* ext);
char*  nova_load_file_to_arena(NovaArena* arena, const char* path, size_t* out_size);

int           nova_run(int argc, const char** argv);
NovaTokenType nova_lookup_keyword(const char* src, size_t len);

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
// C++ Extensions
// ============================================================================

#ifdef __cplusplus
}
#include <memory>
#include <string_view>

namespace nova {

class Arena {
    struct Deleter { void operator()(NovaArena* a) const { nova_arena_destroy(a); } };
    std::unique_ptr<NovaArena, Deleter> handle_;
public:
    explicit Arena(size_t initial = 65536)
        : handle_(nova_arena_create(initial)) { if (!handle_) throw std::bad_alloc(); }
    void* alloc(size_t size) const { return nova_arena_alloc(handle_.get(), size); }
    char* strdup(const char* s) const { return nova_arena_strdup(handle_.get(), s); }
    char* strdup(std::string_view sv) const {
        char* p = static_cast<char*>(alloc(sv.size() + 1));
        if (p) { memcpy(p, sv.data(), sv.size()); p[sv.size()] = '\0'; }
        return p;
    }
    NovaArena* get() const { return handle_.get(); }
};

inline NovaTokenStream tokenize(Arena& arena, std::string_view source) {
    return nova_tokenize(arena.get(), source.data(), source.size());
}

inline std::pair<char*, size_t> load_file(Arena& arena, const char* path) {
    size_t size = 0;
    char* data = nova_load_file_to_arena(arena.get(), path, &size);
    if (!data) throw std::runtime_error("Failed to load file: " + std::string(path));
    return {data, size};
}

namespace debug {
    const char* token_type_name(NovaTokenType t);
    void print_tokens(NovaTokenStream tokens);
    void print_ast(const NovaNode* node, int indent = 0);
}

} // namespace nova
#endif // __cplusplus