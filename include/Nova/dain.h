// dain.h - Unified header for Dain programming language core
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
    DAIN_TOKEN_STRING,
    DAIN_TOKEN_NUMBER,
    DAIN_TOKEN_HEXADECIMAL,
    DAIN_TOKEN_OCTAL,
    DAIN_TOKEN_BINARY,
    DAIN_TOKEN_FLOAT,
    DAIN_TOKEN_IDENTIFIER,

    // ------------------------------------------------------------------------
    // Operadores aritméticos e lógicos
    // ------------------------------------------------------------------------
    DAIN_TOKEN_PLUS,
    DAIN_TOKEN_MINUS,
    DAIN_TOKEN_MULTIPLY,
    DAIN_TOKEN_DIVIDE,
    DAIN_TOKEN_MOD,

    DAIN_TOKEN_AND,
    DAIN_TOKEN_OR,
    DAIN_TOKEN_NOT,

    // ------------------------------------------------------------------------
    // Operadores de comparação
    // ------------------------------------------------------------------------
    DAIN_TOKEN_EQUAL,
    DAIN_TOKEN_NOT_EQUAL,
    DAIN_TOKEN_LESS_THAN,
    DAIN_TOKEN_GREATER_THAN,
    DAIN_TOKEN_LESS_THAN_OR_EQUAL,
    DAIN_TOKEN_GREATER_THAN_OR_EQUAL,

    // ------------------------------------------------------------------------
    // Operadores de atribuição
    // ------------------------------------------------------------------------
    DAIN_TOKEN_ASSIGNMENT,
    DAIN_TOKEN_DECLARATION,     // :=
    DAIN_TOKEN_PLUS_EQUAL,
    DAIN_TOKEN_MINUS_EQUAL,
    DAIN_TOKEN_MULTIPLY_EQUAL,
    DAIN_TOKEN_DIVIDE_EQUAL,

    // ------------------------------------------------------------------------
    // Operadores especiais
    // ------------------------------------------------------------------------
    DAIN_TOKEN_QUESTION,        // ?  optional
    DAIN_TOKEN_BANG,            // !  type may fail
    DAIN_TOKEN_ARROW,           // -> encadeamento de funções

    // ------------------------------------------------------------------------
    // Delimitadores
    // ------------------------------------------------------------------------
    DAIN_TOKEN_LPAREN,
    DAIN_TOKEN_RPAREN,
    DAIN_TOKEN_LBRACE,
    DAIN_TOKEN_RBRACE,
    DAIN_TOKEN_LBRACKET,
    DAIN_TOKEN_RBRACKET,
    DAIN_TOKEN_DOT,
    DAIN_TOKEN_DOTS,            // ...
    DAIN_TOKEN_COMMA,
    DAIN_TOKEN_COLON,
    DAIN_TOKEN_SEMICOLON,

    // ------------------------------------------------------------------------
    // Keywords: controle de fluxo
    // ------------------------------------------------------------------------
    DAIN_TOKEN_IF,
    DAIN_TOKEN_ELSE,
    DAIN_TOKEN_FOR,
    DAIN_TOKEN_IN,
    DAIN_TOKEN_WHILE,           // reservado na ABI, não usado como keyword ativa
    DAIN_TOKEN_SWITCH,
    DAIN_TOKEN_RETURN,
    DAIN_TOKEN_BREAK,
    DAIN_TOKEN_CONTINUE,
    DAIN_TOKEN_GOTO,
    DAIN_TOKEN_MARKER,
    DAIN_TOKEN_SCENE,

    // ------------------------------------------------------------------------
    // Keywords: concorrência / fluxo assíncrono
    // ------------------------------------------------------------------------
    DAIN_TOKEN_SPAWN,
    DAIN_TOKEN_JOINED,
    DAIN_TOKEN_AWAIT,

    // ------------------------------------------------------------------------
    // Keywords: tratamento de erros
    // ------------------------------------------------------------------------
    DAIN_TOKEN_TRY,
    DAIN_TOKEN_CATCH,
    DAIN_TOKEN_MUST,            // "must!" — o ! é semântico, o Parser resolve

    // ------------------------------------------------------------------------
    // Modificadores de propriedade e escopo
    // ------------------------------------------------------------------------
    DAIN_TOKEN_CONST,
    DAIN_TOKEN_MUTABLE,         // keyword: 'mut'
    DAIN_TOKEN_VAR,
    DAIN_TOKEN_LET,
    DAIN_TOKEN_AUTO,

    DAIN_TOKEN_GLOBAL,
    DAIN_TOKEN_PERSISTENT,
    DAIN_TOKEN_LOCAL,
    DAIN_TOKEN_LEND,
    DAIN_TOKEN_SHARED,
    DAIN_TOKEN_VIEW,
    DAIN_TOKEN_UNIQUE,
    DAIN_TOKEN_PACK,            // reservado na ABI; [] é resolvido pelo Parser

    // ------------------------------------------------------------------------
    // Modificadores de acesso
    // ------------------------------------------------------------------------
    DAIN_TOKEN_MODIFIER,        // public / private / protected

    // ------------------------------------------------------------------------
    // Declarações de tipo
    // ------------------------------------------------------------------------
    DAIN_TOKEN_TYPE,
    DAIN_TOKEN_STRUCT,
    DAIN_TOKEN_COMPONENT,
    DAIN_TOKEN_ENUM,
    DAIN_TOKEN_UNION,
    DAIN_TOKEN_FAMILY,
    DAIN_TOKEN_ENTITY,
    DAIN_TOKEN_TRAIT,
    DAIN_TOKEN_TYPEDEF,
    DAIN_TOKEN_IMPLEMENT,

    // ------------------------------------------------------------------------
    // Tokens especiais / controle
    // ------------------------------------------------------------------------
    DAIN_TOKEN_END,
    DAIN_TOKEN_UNKNOWN
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

NovaTokenNovaTokenStream dain_tokenize(NovaArena* arena, const char* source, size_t source_len);

// ============================================================================
// AST System
// ============================================================================

typedef uint16_t NovaNodeId;

enum {
    DAIN_NODE_ERROR = 0,

    DAIN_NODE_LITERAL    = 100,
    DAIN_NODE_IDENTIFIER = 101,
    DAIN_NODE_BINARY_OP  = 102,
    DAIN_NODE_UNARY_OP   = 103,
    DAIN_NODE_CALL       = 104,
    DAIN_NODE_INDEX      = 105,
    DAIN_NODE_MEMBER     = 106,

    DAIN_NODE_VAR_DECL   = 200,
    DAIN_NODE_FUNC_DECL  = 201,
    DAIN_NODE_PARAM      = 202,

    DAIN_NODE_BLOCK      = 300,
    DAIN_NODE_IF         = 301,
    DAIN_NODE_FOR        = 302,  // unifica for e while
    DAIN_NODE_RETURN     = 303,
    DAIN_NODE_EXPR_STMT  = 304,

    DAIN_NODE_TYPE_REF   = 400,
    DAIN_NODE_TYPE_FUNC  = 401,

    DAIN_NODE_CUSTOM_START = 1000
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

NovaNode* dain_parse(NovaArena* arena, NovaTokenStream tokens);
static inline NovaNodeId dain_node_type(const NovaNode* node) {
        return node ? node->type : (NovaNodeId)DAIN_NODE_ERROR;
}

// ============================================================================
// Memory Arena
// ============================================================================

NovaArena* dain_arena_create(size_t initial_block_size);
void*      dain_arena_alloc(NovaArena* arena, size_t size);
char*      dain_arena_strdup(NovaArena* arena, const char* str);
void       dain_arena_reset(NovaArena* arena);
void       dain_arena_destroy(NovaArena* arena);

// ============================================================================
// File Utilities
// ============================================================================

bool   dain_file_exists(const char* path);
bool   dain_file_is_regular(const char* path);
size_t dain_file_size(const char* path);
bool   dain_file_has_extension(const char* path, const char* ext);
char*  dain_load_file_to_arena(NovaArena* arena, const char* path, size_t* out_size);

int           dain_run(int argc, const char** argv);
NovaTokenType dain_lookup_keyword(const char* src, size_t len);

// ============================================================================
// Error Handling
// ============================================================================

typedef enum {
    DAIN_OK = 0,
    DAIN_ERR_IO,
    DAIN_ERR_PARSE,
    DAIN_ERR_LEX,
    DAIN_ERR_MEMORY,
    DAIN_ERR_INVALID_INPUT
} NovaError;

// ============================================================================
// C++ Extensions
// ============================================================================

#ifdef __cplusplus
}
#include <memory>
#include <string_view>

namespace DAIN {

class Arena {
    struct Deleter { void operator()(NovaArena* a) const { dain_arena_destroy(a); } };
    std::unique_ptr<NovaArena, Deleter> handle_;
public:
    explicit Arena(size_t initial = 65536)
        : handle_(dain_arena_create(initial)) { if (!handle_) throw std::bad_alloc(); }
    void* alloc(size_t size) const { return dain_arena_alloc(handle_.get(), size); }
    char* strdup(const char* s) const { return dain_arena_strdup(handle_.get(), s); }
    char* strdup(std::string_view sv) const {
        char* p = static_cast<char*>(alloc(sv.size() + 1));
        if (p) { memcpy(p, sv.data(), sv.size()); p[sv.size()] = '\0'; }
        return p;
    }
    NovaArena* get() const { return handle_.get(); }
};

inline NovaTokenStream tokenize(Arena& arena, std::string_view source) {
    return dain_tokenize(arena.get(), source.data(), source.size());
}

inline std::pair<char*, size_t> load_file(Arena& arena, const char* path) {
    size_t size = 0;
    char* data = dain_load_file_to_arena(arena.get(), path, &size);
    if (!data) throw std::runtime_error("Failed to load file: " + std::string(path));
    return {data, size};
}

namespace debug {
    const char* token_type_name(NovaTokenType t);
    void print_tokens(NovaTokenStream tokens);
    void print_ast(const NovaNode* node, int indent = 0);
}

} // namespace DAIN
#endif // __cplusplus