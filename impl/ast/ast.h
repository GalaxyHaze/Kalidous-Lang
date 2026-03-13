// kalidous_ast.h — AST node definitions, constructors and visitors
#pragma once

#include <Kalidous/kalidous.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Node IDs estendidos
//
//   100–199  Expressões
//   200–299  Declarações
//   300–399  Statements
//   400–499  Tipos
//   500–599  Raiz / auxiliares
//
// ============================================================================

enum {
    // -- Expressões (100–199) ------------------------------------------------
    KALIDOUS_NODE_ARROW_CALL    = 107,  // a->b(...)   encadeamento
    KALIDOUS_NODE_CAST          = 108,  // expr as Type
    KALIDOUS_NODE_OPTIONAL      = 109,  // expr?
    KALIDOUS_NODE_UNWRAP        = 110,  // expr!  (must unwrap)
    KALIDOUS_NODE_RANGE         = 111,  // a..b  ou  a...b
    KALIDOUS_NODE_LAMBDA        = 112,  // |params| -> expr
    KALIDOUS_NODE_SPAWN_EXPR    = 113,  // spawn expr
    KALIDOUS_NODE_RECURSE       = 114,  // recurse(args)  → CPS transform

    // -- Literais compostos (150–199) ----------------------------------------
    KALIDOUS_NODE_ARRAY_LIT     = 150,  // [a, b, c]
    KALIDOUS_NODE_STRUCT_LIT    = 151,  // Point { x: 1, y: 2 }
    KALIDOUS_NODE_TUPLE_LIT     = 152,  // (a, b, c)

    // -- Declarações (200–299) -----------------------------------------------
    KALIDOUS_NODE_CONST_DECL    = 203,  // const X = ...
    KALIDOUS_NODE_STRUCT_DECL   = 204,  // struct Foo { ... }
    KALIDOUS_NODE_ENUM_DECL     = 205,  // enum Color { ... }
    KALIDOUS_NODE_TRAIT_DECL    = 206,  // trait Drawable { ... }
    KALIDOUS_NODE_IMPL_DECL     = 207,  // implement Drawable for Foo { ... }
    KALIDOUS_NODE_TYPE_ALIAS    = 208,  // using NewName = OldType
    KALIDOUS_NODE_COMPONENT_DECL= 209,  // component Foo { ... }
    KALIDOUS_NODE_UNION_DECL    = 210,  // union Bar { ... }
    KALIDOUS_NODE_FAMILY_DECL   = 211,  // family (sum type / tagged union)
    KALIDOUS_NODE_ENTITY_DECL   = 212,  // entity (ECS-style)
    KALIDOUS_NODE_MODULE_DECL   = 213,  // module foo { ... }
    KALIDOUS_NODE_IMPORT        = 214,  // use foo::bar

    // -- Statements (300–399) ------------------------------------------------
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
    KALIDOUS_NODE_YIELD         = 316,  // yield expr  (em async fn)

    // -- Tipos (400–499) -----------------------------------------------------
    KALIDOUS_NODE_TYPE_OPTIONAL = 402,  // Type?
    KALIDOUS_NODE_TYPE_RESULT   = 403,  // Type!
    KALIDOUS_NODE_TYPE_ARRAY    = 404,  // [N]Type  ou  []Type
    KALIDOUS_NODE_TYPE_TUPLE    = 405,  // (A, B, C)
    KALIDOUS_NODE_TYPE_POINTER  = 406,  // *Type  (raw)
    KALIDOUS_NODE_TYPE_UNIQUE   = 407,  // unique Type
    KALIDOUS_NODE_TYPE_SHARED   = 408,  // shared Type
    KALIDOUS_NODE_TYPE_VIEW     = 409,  // view Type   (borrow imutável)
    KALIDOUS_NODE_TYPE_LEND     = 410,  // lend Type   (borrow mutável)
    KALIDOUS_NODE_TYPE_PACK     = 411,  // pack Type   (variadic / slice)

    // -- Raiz / auxiliares (500–599) -----------------------------------------
    KALIDOUS_NODE_PROGRAM       = 500,  // nó raiz — lista de top-level decls
    KALIDOUS_NODE_FIELD         = 501,  // campo de struct/component/entity
    KALIDOUS_NODE_ENUM_VARIANT  = 502,  // variante de enum/family
    KALIDOUS_NODE_MATCH_ARM     = 503,  // braço de switch/match
};

// ============================================================================
// Enums auxiliares
// ============================================================================

// Tipo de função — determina as restrições que o parser/sema aplicam ao corpo
typedef enum KalidousFnKind {
    KALIDOUS_FN_NORMAL   = 0,   // fn          — função síncrona normal
    KALIDOUS_FN_ASYNC    = 1,   // async fn    — coroutine; pode usar yield
    KALIDOUS_FN_NORETURN = 2,   // noreturn fn — tail-call; usa goto/marker, sem return
    KALIDOUS_FN_FLOWING  = 3,   // flowing fn  — pode usar goto; tem return normal
} KalidousFnKind;

// Modo de binding de variável — substitui os bools soltos em VarDeclData
typedef enum KalidousBindingKind {
    KALIDOUS_BINDING_LET,        // let   — imutável, local
    KALIDOUS_BINDING_VAR,        // var   — mutável,  local
    KALIDOUS_BINDING_CONST,      // const — imutável, compilação
    KALIDOUS_BINDING_AUTO,       // auto  — tipo inferido
    KALIDOUS_BINDING_GLOBAL,     // global
    KALIDOUS_BINDING_PERSISTENT, // persistent — sobrevive ao scope (static-like)
    KALIDOUS_BINDING_LOCAL,      // local — hint de stack, sem escape
} KalidousBindingKind;

// Modificador de ownership para parâmetros e variáveis
typedef enum KalidousOwnership {
    KALIDOUS_OWN_DEFAULT,   // por valor / owned (padrão)
    KALIDOUS_OWN_VIEW,      // view  — borrow imutável
    KALIDOUS_OWN_LEND,      // lend  — borrow mutável
    KALIDOUS_OWN_UNIQUE,    // unique — move semântico
    KALIDOUS_OWN_SHARED,    // shared — ref-counted
    KALIDOUS_OWN_PACK,      // pack   — variadic / slice
} KalidousOwnership;

// ============================================================================
// Informação de tipo resolvida (preenchida pela sema; NULL antes disso)
// ============================================================================

typedef struct KalidousType KalidousType;
struct KalidousType {
    KalidousNodeId kind;        // KALIDOUS_NODE_TYPE_*
    const char*    name;        // nome textual (debug / LSP)
    KalidousType*  inner;       // para ponteiros, arrays, optional, etc.
    size_t         array_size;  // 0 = dinâmico / slice
    bool           is_const;
};

// ============================================================================
// Payloads dos nós
// ============================================================================

// Declaração de variável / binding
typedef struct {
    const char*        name;
    size_t             name_len;
    KalidousBindingKind binding;        // kind do binding (let, var, const, ...)
    KalidousOwnership  ownership;       // ownership modifier
    KalidousNode*      type_node;       // NULL se inferido
    KalidousNode*      initializer;     // NULL se sem valor inicial
} KalidousVarDeclData;

// Declaração de função
typedef struct {
    const char*    name;
    size_t         name_len;
    KalidousFnKind kind;                // fn, async fn, noreturn fn, flowing fn
    KalidousNode** params;              // array de KALIDOUS_NODE_PARAM
    size_t         param_count;
    KalidousNode*  return_type;         // NULL = void / inferido
    KalidousNode*  body;                // NULL = declaração forward
    bool           is_public;
    bool           is_extern;
} KalidousFuncDeclData;

// Parâmetro de função
typedef struct {
    const char*       name;
    size_t            name_len;
    KalidousOwnership ownership;        // view, lend, unique, shared, pack
    KalidousNode*     type_node;
    KalidousNode*     default_value;    // NULL se sem default
    bool              is_mutable;
} KalidousParamData;

// Operação binária
typedef struct {
    KalidousTokenType op;               // PLUS, MINUS, EQUAL, etc.
    KalidousNode*     left;
    KalidousNode*     right;
} KalidousBinaryOpData;

// Operação unária
typedef struct {
    KalidousTokenType op;               // MINUS, BANG, QUESTION, ...
    KalidousNode*     operand;
    bool              is_postfix;
} KalidousUnaryOpData;

// Chamada de função (inclui recurse — o parser emite KALIDOUS_NODE_RECURSE
// com o mesmo payload; a sema valida que só ocorre dentro de fn com recurse)
typedef struct {
    KalidousNode*  callee;              // IDENTIFIER ou MEMBER_ACCESS
    KalidousNode** args;
    size_t         arg_count;
} KalidousCallData;

// Literal escalar
typedef struct {
    KalidousTokenType kind;             // STRING, NUMBER, FLOAT, HEX, BINARY, ...
    union {
        int64_t     i64;
        uint64_t    u64;
        float       f32;
        double      f64;
        struct { const char* ptr; size_t len; } string;
        bool        boolean;
    } value;
} KalidousLiteralData;

// Declaração de struct
typedef struct {
    const char*    name;
    size_t         name_len;
    KalidousNode** fields;              // array de KALIDOUS_NODE_FIELD
    size_t         field_count;
    KalidousNode** methods;             // array de KALIDOUS_NODE_FUNC_DECL
    size_t         method_count;
    bool           is_public;
} KalidousStructDeclData;

// Declaração de enum
typedef struct {
    const char*    name;
    size_t         name_len;
    KalidousNode** variants;            // array de KALIDOUS_NODE_ENUM_VARIANT
    size_t         variant_count;
    bool           is_public;
} KalidousEnumDeclData;

// if / else
typedef struct {
    KalidousNode*  condition;
    KalidousNode*  then_branch;
    KalidousNode*  else_branch;         // NULL, outro IF, ou BLOCK
} KalidousIfData;

// for — unifica for-clássico e for-in
// Apenas um dos dois modos estará preenchido; is_for_in distingue-os
typedef struct {
    // Modo for-in:   for x in iterable { body }
    KalidousNode*  iterator_var;        // NULL no modo clássico
    KalidousNode*  iterable;            // NULL no modo clássico
    // Modo clássico: for init; cond; step { body }
    KalidousNode*  init;                // NULL no modo for-in
    KalidousNode*  condition;           // NULL = loop infinito (modo clássico)
    KalidousNode*  step;                // NULL no modo for-in
    // Corpo
    KalidousNode*  body;
    bool           is_for_in;
} KalidousForData;

// switch / match
typedef struct {
    KalidousNode*  subject;
    KalidousNode** arms;                // array de KALIDOUS_NODE_MATCH_ARM
    size_t         arm_count;
    KalidousNode*  default_arm;         // NULL se não houver
} KalidousSwitchData;

// try / catch
typedef struct {
    KalidousNode*  try_block;
    const char*    catch_var;           // nome da variável de erro
    size_t         catch_var_len;
    KalidousNode*  catch_block;
} KalidousTryCatchData;

// import — use foo::bar  /  use foo::*
typedef struct {
    const char**   path;                // segmentos: ["std", "io", "File"]
    size_t         path_len;
    const char*    alias;               // NULL se sem alias
    bool           is_wildcard;         // use foo::*
} KalidousImportData;

// ============================================================================
// Construtores (alocam na arena)
// ============================================================================

KalidousNode* kalidous_ast_make_program    (KalidousArena* a, KalidousNode** decls, size_t count);
KalidousNode* kalidous_ast_make_literal    (KalidousArena* a, KalidousSourceLoc loc, KalidousLiteralData lit);
KalidousNode* kalidous_ast_make_identifier (KalidousArena* a, KalidousSourceLoc loc, const char* name, size_t len);
KalidousNode* kalidous_ast_make_binary_op  (KalidousArena* a, KalidousSourceLoc loc, KalidousBinaryOpData op);
KalidousNode* kalidous_ast_make_unary_op   (KalidousArena* a, KalidousSourceLoc loc, KalidousUnaryOpData op);
KalidousNode* kalidous_ast_make_call       (KalidousArena* a, KalidousSourceLoc loc, KalidousCallData call);
KalidousNode* kalidous_ast_make_recurse    (KalidousArena* a, KalidousSourceLoc loc, KalidousCallData call);
KalidousNode* kalidous_ast_make_var_decl   (KalidousArena* a, KalidousSourceLoc loc, KalidousVarDeclData decl);
KalidousNode* kalidous_ast_make_func_decl  (KalidousArena* a, KalidousSourceLoc loc, KalidousFuncDeclData decl);
KalidousNode* kalidous_ast_make_param      (KalidousArena* a, KalidousSourceLoc loc, KalidousParamData param);
KalidousNode* kalidous_ast_make_block      (KalidousArena* a, KalidousSourceLoc loc, KalidousNode** stmts, size_t count);
KalidousNode* kalidous_ast_make_if         (KalidousArena* a, KalidousSourceLoc loc, KalidousIfData data);
KalidousNode* kalidous_ast_make_for        (KalidousArena* a, KalidousSourceLoc loc, KalidousForData data);
KalidousNode* kalidous_ast_make_return     (KalidousArena* a, KalidousSourceLoc loc, KalidousNode* value);
KalidousNode* kalidous_ast_make_yield      (KalidousArena* a, KalidousSourceLoc loc, KalidousNode* value);
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

void kalidous_ast_walk(KalidousNode* root,
                       KalidousASTVisitorFn pre,
                       KalidousASTVisitorFn post,
                       void* userdata);

// ============================================================================
// Debug
// ============================================================================

void        kalidous_ast_print     (const KalidousNode* node, int indent);
const char* kalidous_ast_node_name (KalidousNodeId id);
const char* kalidous_ast_fn_kind_name (KalidousFnKind kind);

// ============================================================================
// TODO — Futuros
// ============================================================================

// TODO: kalidous_ast_clone(arena, node)              — deep copy de um nó
// TODO: kalidous_ast_fold_constants(arena, node)     — constant folding no AST
// TODO: kalidous_ast_resolve_types(arena, node, ctx) — anotação de tipos (sema)
// TODO: kalidous_ast_serialize / deserialize         — cache / LSP
// TODO: span tracking com end (start+end, não só start)
// TODO: comment nodes para doc-comments (ferramenta docs)
// TODO: generic params + where clauses (constraints de traits)
// TODO: macro / comptime nodes
// TODO: selective imports: use foo::{A, B, C}
// TODO: multiple catch por tipo de erro + finally
// TODO: named arguments: foo(x: 1, y: 2)
// TODO: trailing lambda: foo { ... }
// TODO: derive list: #[derive(Debug, Clone)]
// TODO: layout hints: #[packed], #[align(N)]
// TODO: backing type em enum: enum Foo: u8 { ... }

#ifdef __cplusplus
}
#endif