// kalidous_ast.h - AST node definitions, constructors and visitors
#pragma once

#include <Kalidous/kalidous.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Node IDs estendidos (além dos definidos em kalidous.h)
// ============================================================================

enum {
    // -- Expressões --
    KALIDOUS_NODE_ARROW_CALL    = 107,  // a -> b(...)   encadeamento
    KALIDOUS_NODE_CAST          = 108,  // expr as Type
    KALIDOUS_NODE_OPTIONAL      = 109,  // expr?
    KALIDOUS_NODE_UNWRAP        = 110,  // expr!  (must unwrap)
    KALIDOUS_NODE_RANGE         = 111,  // a..b  ou  a...b
    KALIDOUS_NODE_LAMBDA        = 112,  // |params| -> expr
    KALIDOUS_NODE_SPAWN_EXPR    = 113,  // spawn expr

    // -- Literais compostos --
    KALIDOUS_NODE_ARRAY_LIT     = 150,  // [a, b, c]
    KALIDOUS_NODE_STRUCT_LIT    = 151,  // Point { x: 1, y: 2 }
    KALIDOUS_NODE_TUPLE_LIT     = 152,  // (a, b, c)

    // -- Declarações --
    KALIDOUS_NODE_CONST_DECL    = 203,  // const X = ...
    KALIDOUS_NODE_STRUCT_DECL   = 204,  // struct Foo { ... }
    KALIDOUS_NODE_ENUM_DECL     = 205,  // enum Color { Red, Green, Blue }
    KALIDOUS_NODE_TRAIT_DECL    = 206,  // trait Drawable { ... }
    KALIDOUS_NODE_IMPL_DECL     = 207,  // implement Drawable for Foo { ... }
    KALIDOUS_NODE_TYPE_ALIAS    = 208,  // typedef NewName = OldType
    KALIDOUS_NODE_COMPONENT_DECL= 209,  // component Foo { ... }
    KALIDOUS_NODE_UNION_DECL    = 210,  // union Bar { ... }
    KALIDOUS_NODE_FAMILY_DECL   = 211,  // family (sum type / tagged union)
    KALIDOUS_NODE_ENTITY_DECL   = 212,  // entity (ECS-style)
    KALIDOUS_NODE_MODULE_DECL   = 213,  // module foo { ... }
    KALIDOUS_NODE_IMPORT        = 214,  // use foo::bar

    // -- Statements --
    KALIDOUS_NODE_SWITCH        = 305,  // switch expr { ... }
    KALIDOUS_NODE_CASE          = 306,  // case pattern: ...
    KALIDOUS_NODE_BREAK         = 307,
    KALIDOUS_NODE_CONTINUE      = 308,
    KALIDOUS_NODE_GOTO          = 309,
    KALIDOUS_NODE_MARKER        = 310,  // marker label:
    KALIDOUS_NODE_SCENE         = 311,  // scene { ... }
    KALIDOUS_NODE_TRY_CATCH     = 312,  // try { } catch e { }
    KALIDOUS_NODE_SPAWN_STMT    = 313,  // spawn { ... }
    KALIDOUS_NODE_AWAIT_STMT    = 314,  // await expr
    KALIDOUS_NODE_JOINED        = 315,  // joined { ... }

    // -- Tipos --
    KALIDOUS_NODE_TYPE_OPTIONAL = 402,  // Type?
    KALIDOUS_NODE_TYPE_RESULT   = 403,  // Type!
    KALIDOUS_NODE_TYPE_ARRAY    = 404,  // [N]Type  ou  []Type
    KALIDOUS_NODE_TYPE_TUPLE    = 405,  // (A, B, C)
    KALIDOUS_NODE_TYPE_POINTER  = 406,  // *Type  (raw)
    KALIDOUS_NODE_TYPE_UNIQUE   = 407,  // unique Type
    KALIDOUS_NODE_TYPE_SHARED   = 408,  // shared Type
    KALIDOUS_NODE_TYPE_VIEW     = 409,  // view Type  (borrow)
    KALIDOUS_NODE_TYPE_LEND     = 410,  // lend Type  (mut borrow)
    KALIDOUS_NODE_TYPE_PACK     = 411,  // pack Type  (variadic / slice)

    // -- Raiz --
    KALIDOUS_NODE_PROGRAM       = 500,  // nó raiz — lista de top-level decls
    KALIDOUS_NODE_FIELD         = 501,  // campo de struct/component/entity
    KALIDOUS_NODE_ENUM_VARIANT  = 502,  // variante de enum/family
    KALIDOUS_NODE_MATCH_ARM     = 503,  // braço de switch/match
};

// ============================================================================
// Informação de tipo resolvida (preenchida pela sema, NULL antes disso)
// ============================================================================

typedef struct KalidousType KalidousType;
struct KalidousType {
    KalidousNodeId kind;            // KALIDOUS_NODE_TYPE_*
    const char*    name;            // nome textual (debug)
    KalidousType*  inner;           // para ponteiros, arrays, optional, etc.
    size_t         array_size;      // 0 = dinâmico / slice
    bool           is_const;
    // TODO: generic params quando houver suporte a generics
};

// ============================================================================
// Nó AST estendido
// Usa o KalidousNode base mas com payload extra acessível via helpers
// ============================================================================

// Payload para declaração de variável
typedef struct {
    const char*    name;
    size_t         name_len;
    KalidousNode*  type_node;       // NULL se inferido
    KalidousNode*  initializer;     // NULL se sem valor inicial
    bool           is_mutable;
    bool           is_const;
    bool           is_global;
    bool           is_persistent;
    // TODO: atributos / anotações (#[inline], #[deprecated], ...)
} KalidousVarDeclData;

// Payload para declaração de função
typedef struct {
    const char*    name;
    size_t         name_len;
    KalidousNode** params;          // array de KALIDOUS_NODE_PARAM
    size_t         param_count;
    KalidousNode*  return_type;     // NULL = void / inferido
    KalidousNode*  body;            // NULL = declaração forward
    bool           is_public;
    bool           is_extern;
    // TODO: atributos: #[inline], #[cold], #[no_mangle], #[export]
    // TODO: generics / type params
    // TODO: where clauses (constraints de traits)
} KalidousFuncDeclData;

// Payload para parâmetro de função
typedef struct {
    const char*    name;
    size_t         name_len;
    KalidousNode*  type_node;
    KalidousNode*  default_value;   // NULL se sem default
    bool           is_mutable;
    // TODO: ownership modifier: view, lend, unique, shared, pack
} KalidousParamData;

// Payload para operação binária
typedef struct {
    KalidousTokenType op;           // PLUS, MINUS, EQUAL, etc.
    KalidousNode*     left;
    KalidousNode*     right;
} KalidousBinaryOpData;

// Payload para operação unária
typedef struct {
    KalidousTokenType op;           // MINUS (negate), NOT, BANG, QUESTION
    KalidousNode*     operand;
    bool              is_postfix;
} KalidousUnaryOpData;

// Payload para chamada de função
typedef struct {
    KalidousNode*  callee;          // IDENTIFIER ou MEMBER
    KalidousNode** args;
    size_t         arg_count;
    // TODO: named arguments: foo(x: 1, y: 2)
    // TODO: trailing lambda: foo { ... }
} KalidousCallData;

// Payload para literais
typedef struct {
    KalidousTokenType kind;         // STRING, NUMBER, FLOAT, HEXADECIMAL, etc.
    union {
        double      number;
        int64_t     integer;
        uint64_t    uinteger;
        float       f32;
        double      f64;
        struct { const char* ptr; size_t len; } string;
        bool        boolean;
    } value;
} KalidousLiteralData;

// Payload para struct
typedef struct {
    const char*    name;
    size_t         name_len;
    KalidousNode** fields;          // array de KALIDOUS_NODE_FIELD
    size_t         field_count;
    KalidousNode** methods;         // array de KALIDOUS_NODE_FUNC_DECL (inline impl)
    size_t         method_count;
    bool           is_public;
    // TODO: derive list: #[derive(Debug, Clone)]
    // TODO: generic params
    // TODO: layout hints: #[packed], #[align(N)]
} KalidousStructDeclData;

// Payload para enum
typedef struct {
    const char*    name;
    size_t         name_len;
    KalidousNode** variants;        // array de KALIDOUS_NODE_ENUM_VARIANT
    size_t         variant_count;
    bool           is_public;
    // TODO: backing type (ex: enum Foo: u8 { ... })
    // TODO: family = tagged union com payload por variante
} KalidousEnumDeclData;

// Payload para if/else
typedef struct {
    KalidousNode*  condition;
    KalidousNode*  then_branch;
    KalidousNode*  else_branch;     // NULL, outro IF, ou BLOCK
} KalidousIfData;

// Payload para for (unifica for e while)
typedef struct {
    KalidousNode*  init;            // NULL para while-style
    KalidousNode*  condition;       // NULL = infinito
    KalidousNode*  step;            // NULL para while-style
    KalidousNode*  iterator_var;    // para "for x in collection"
    KalidousNode*  iterable;        // para "for x in collection"
    KalidousNode*  body;
    bool           is_for_in;
} KalidousForData;

// Payload para switch
typedef struct {
    KalidousNode*  subject;
    KalidousNode** arms;            // array de KALIDOUS_NODE_MATCH_ARM
    size_t         arm_count;
    KalidousNode*  default_arm;     // NULL se não houver
    // TODO: exhaustiveness check na sema
} KalidousSwitchData;

// Payload para try/catch
typedef struct {
    KalidousNode*  try_block;
    const char*    catch_var;       // nome da variável de erro
    size_t         catch_var_len;
    KalidousNode*  catch_block;
    // TODO: múltiplos catch por tipo de erro
    // TODO: finally block
} KalidousTryCatchData;

// Payload para import
typedef struct {
    const char**   path;            // segmentos: ["std", "io", "File"]
    size_t         path_len;
    const char*    alias;           // NULL se sem alias
    bool           is_wildcard;     // use foo::*
    // TODO: selective imports: use foo::{A, B, C}
} KalidousImportData;

// ============================================================================
// Construtores de nós (alocam na arena)
// ============================================================================

KalidousNode* kalidous_ast_make_program    (KalidousArena* a, KalidousNode** decls, size_t count);
KalidousNode* kalidous_ast_make_literal    (KalidousArena* a, KalidousSourceLoc loc, KalidousLiteralData lit);
KalidousNode* kalidous_ast_make_identifier (KalidousArena* a, KalidousSourceLoc loc, const char* name, size_t len);
KalidousNode* kalidous_ast_make_binary_op  (KalidousArena* a, KalidousSourceLoc loc, KalidousBinaryOpData op);
KalidousNode* kalidous_ast_make_unary_op   (KalidousArena* a, KalidousSourceLoc loc, KalidousUnaryOpData op);
KalidousNode* kalidous_ast_make_call       (KalidousArena* a, KalidousSourceLoc loc, KalidousCallData call);
KalidousNode* kalidous_ast_make_var_decl   (KalidousArena* a, KalidousSourceLoc loc, KalidousVarDeclData decl);
KalidousNode* kalidous_ast_make_func_decl  (KalidousArena* a, KalidousSourceLoc loc, KalidousFuncDeclData decl);
KalidousNode* kalidous_ast_make_param      (KalidousArena* a, KalidousSourceLoc loc, KalidousParamData param);
KalidousNode* kalidous_ast_make_block      (KalidousArena* a, KalidousSourceLoc loc, KalidousNode** stmts, size_t count);
KalidousNode* kalidous_ast_make_if         (KalidousArena* a, KalidousSourceLoc loc, KalidousIfData data);
KalidousNode* kalidous_ast_make_for        (KalidousArena* a, KalidousSourceLoc loc, KalidousForData data);
KalidousNode* kalidous_ast_make_return     (KalidousArena* a, KalidousSourceLoc loc, KalidousNode* value);
KalidousNode* kalidous_ast_make_struct     (KalidousArena* a, KalidousSourceLoc loc, KalidousStructDeclData decl);
KalidousNode* kalidous_ast_make_enum       (KalidousArena* a, KalidousSourceLoc loc, KalidousEnumDeclData decl);
KalidousNode* kalidous_ast_make_switch     (KalidousArena* a, KalidousSourceLoc loc, KalidousSwitchData data);
KalidousNode* kalidous_ast_make_try_catch  (KalidousArena* a, KalidousSourceLoc loc, KalidousTryCatchData data);
KalidousNode* kalidous_ast_make_import     (KalidousArena* a, KalidousSourceLoc loc, KalidousImportData data);
KalidousNode* kalidous_ast_make_error      (KalidousArena* a, KalidousSourceLoc loc, const char* msg);

// ============================================================================
// Visitor / Walker
// ============================================================================

// Retorna false para parar o traversal
typedef bool (*KalidousASTVisitorFn)(KalidousNode* node, void* userdata);

void kalidous_ast_walk(KalidousNode* root, KalidousASTVisitorFn pre, KalidousASTVisitorFn post, void* userdata);

// ============================================================================
// Debug
// ============================================================================

void kalidous_ast_print(const KalidousNode* node, int indent);
const char* kalidous_ast_node_name(KalidousNodeId id);

// ============================================================================
// TODO - Futuros
// ============================================================================

// TODO: kalidous_ast_clone(arena, node)  — deep copy de um nó
// TODO: kalidous_ast_fold_constants(arena, node) — constant folding no AST
// TODO: kalidous_ast_resolve_types(arena, node, sema_ctx) — anotação de tipos
// TODO: kalidous_ast_serialize(node, buf)   — serialização para cache/LSP
// TODO: kalidous_ast_deserialize(arena, buf) — deserialização
// TODO: span tracking melhorado (start + end, não só start)
// TODO: comment nodes para preservar doc-comments no AST (para 'docs')
// TODO: macro / comptime nodes quando o sistema de macros for definido

#ifdef __cplusplus
}
#endif