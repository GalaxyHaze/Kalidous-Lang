// kalidous_parser.cpp - Recursive descent parser for Kalidous
#include <Kalidous/kalidous.h>
#include "ast.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// Parser State
// ============================================================================

struct Parser {
    KalidousArena*       arena;
    const KalidousToken* tokens;
    size_t               count;
    size_t               pos;
    bool                 had_error;

    // TODO: sistema de diagnóstico completo:
    //   - acumular múltiplos erros antes de abortar
    //   - recovery points para continuar após erros
    //   - error spans com linha:coluna de início e fim
    //   - níveis: error / warning / note / hint
    //   - sugestões de fix ("did you mean X?")
};

// ============================================================================
// Helpers básicos
// ============================================================================

static const KalidousToken* peek(const Parser* p) {
    if (p->pos < p->count) return &p->tokens[p->pos];
    // Sentinel — token END
    static constexpr KalidousToken eof = { {nullptr, 0}, {0, 0}, KALIDOUS_TOKEN_END, 0 };
    return &eof;
}

static const KalidousToken* peek_ahead(const Parser* p, size_t offset) {
    if (const size_t idx = p->pos + offset; idx < p->count) return &p->tokens[idx];
    static constexpr KalidousToken eof = { {nullptr, 0}, {0, 0}, KALIDOUS_TOKEN_END, 0 };
    return &eof;
}

static const KalidousToken* advance(Parser* p) {
    const KalidousToken* t = peek(p);
    if (p->pos < p->count) p->pos++;
    return t;
}

static bool check(const Parser* p, const KalidousTokenType type) {
    return peek(p)->type == type;
}

static bool match(Parser* p, const KalidousTokenType type) {
    if (check(p, type)) { advance(p); return true; }
    return false;
}

static const KalidousToken* expect(Parser* p, const KalidousTokenType type, const char* msg) {
    if (check(p, type)) return advance(p);
    const KalidousToken* t = peek(p);
    fprintf(stderr, "[parse error] line %zu: %s (got token type %d)\n",
            t->loc.line, msg, static_cast<int>(t->type));
    p->had_error = true;

    // TODO: error recovery — skip tokens até encontrar um synchronization point
    //   pontos de sincronização: ';', '}', keywords de topo (fn, struct, enum...)
    // TODO: sugerir o token esperado no output do erro
    return t;
}

static bool is_at_end(const Parser* p) {
    return peek(p)->type == KALIDOUS_TOKEN_END;
}

// ============================================================================
// Forward declarations
// ============================================================================

static KalidousNode* parse_declaration(Parser* p);
static KalidousNode* parse_statement(Parser* p);
static KalidousNode* parse_expression(Parser* p);
static KalidousNode* parse_type(Parser* p);
static KalidousNode* parse_block(Parser* p);

// ============================================================================
// Tipos
// ============================================================================

static KalidousNode* parse_type(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;

    // optional: Type?     (prefixo ? antes do tipo base, ou sufixo — a definir)
    // result:   Type!
    // pointer:  *Type
    // unique:   unique Type
    // shared:   shared Type
    // view:     view   Type
    // lend:     lend   Type
    // array:    [N]Type  ou  []Type

    // TODO: implementar parse completo de tipos compostos
    //   - ownership modifiers (unique, shared, view, lend)
    //   - pointer types (*Type)
    //   - array types ([N]Type, []Type)
    //   - tuple types (A, B, C)
    //   - function types fn(A, B) -> C
    //   - optional Type?
    //   - result Type!
    //   - generic instantiation Type<A, B>

    if (check(p, KALIDOUS_TOKEN_IDENTIFIER)) {
        const KalidousToken* t = advance(p);
        return kalidous_ast_make_identifier(p->arena, loc, t->lexeme.data, t->lexeme.len);
    }

    fprintf(stderr, "[parse error] line %zu: expected type\n", loc.line);
    p->had_error = true;
    return kalidous_ast_make_error(p->arena, loc, "expected type");
}

// ============================================================================
// Expressões (Pratt parser)
// ============================================================================

// Binding powers (precedência)
typedef struct { int left; int right; } BindingPower;

static BindingPower infix_bp(const KalidousTokenType op) {
    switch (op) {
        case KALIDOUS_TOKEN_OR:                    return {1, 2};
        case KALIDOUS_TOKEN_AND:                   return {3, 4};
        case KALIDOUS_TOKEN_EQUAL:
        case KALIDOUS_TOKEN_NOT_EQUAL:             return {5, 6};
        case KALIDOUS_TOKEN_LESS_THAN:
        case KALIDOUS_TOKEN_GREATER_THAN:
        case KALIDOUS_TOKEN_LESS_THAN_OR_EQUAL:
        case KALIDOUS_TOKEN_GREATER_THAN_OR_EQUAL: return {7, 8};
        case KALIDOUS_TOKEN_PLUS:
        case KALIDOUS_TOKEN_MINUS:                 return {9, 10};
        case KALIDOUS_TOKEN_MULTIPLY:
        case KALIDOUS_TOKEN_DIVIDE:
        case KALIDOUS_TOKEN_MOD:                   return {11, 12};
        case KALIDOUS_TOKEN_ARROW:                 return {13, 14}; // ->
        case KALIDOUS_TOKEN_DOT:                   return {15, 16}; // member access
        default:                                   return {-1, -1};
    }
}

static KalidousNode* parse_expr_bp(Parser* p, int min_bp);

// Parseia prefixos / átomos
static KalidousNode* parse_nud(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;

    switch (const KalidousToken* t = advance(p); t->type) {

        // -- Literais --
        case KALIDOUS_TOKEN_NUMBER: {
            KalidousLiteralData lit;
            lit.kind = KALIDOUS_TOKEN_NUMBER;
            lit.value.number = 0; // TODO: atoi/strtod do lexeme
            return kalidous_ast_make_literal(p->arena, loc, lit);
        }
        case KALIDOUS_TOKEN_FLOAT: {
            KalidousLiteralData lit;
            lit.kind = KALIDOUS_TOKEN_FLOAT;
            lit.value.f64 = 0.0; // TODO: strtod do lexeme
            return kalidous_ast_make_literal(p->arena, loc, lit);
        }
        case KALIDOUS_TOKEN_STRING: {
            KalidousLiteralData lit;
            lit.kind = KALIDOUS_TOKEN_STRING;
            lit.value.string = { t->lexeme.data, t->lexeme.len };
            return kalidous_ast_make_literal(p->arena, loc, lit);
        }
        case KALIDOUS_TOKEN_HEXADECIMAL:
        case KALIDOUS_TOKEN_OCTAL:
        case KALIDOUS_TOKEN_BINARY: {
            // TODO: converter base do lexeme para inteiro
            KalidousLiteralData lit;
            lit.kind = t->type;
            lit.value.uinteger = 0;
            return kalidous_ast_make_literal(p->arena, loc, lit);
        }

        // -- Identificador (pode ser chamada de função) --
        case KALIDOUS_TOKEN_IDENTIFIER: {
            KalidousNode* ident = kalidous_ast_make_identifier(p->arena, loc,
                                      t->lexeme.data, t->lexeme.len);
            // chamada: foo(...)
            if (check(p, KALIDOUS_TOKEN_LPAREN)) {
                advance(p); // consome '('
                // TODO: parsear lista de argumentos (com named args no futuro)
                KalidousNode** args = nullptr;
                constexpr size_t arg_count = 0;
                // Placeholder — a lista real virá quando o allocator de arrays estiver pronto
                // TODO: allocar array de args na arena
                //       while (!check(RPAREN)) { args[i++] = parse_expression(); match(COMMA); }
                expect(p, KALIDOUS_TOKEN_RPAREN, "expected ')' after arguments");
                const KalidousCallData call = { ident, args, arg_count };
                return kalidous_ast_make_call(p->arena, loc, call);
            }
            return ident;
        }

        // -- Unários prefixo --
        case KALIDOUS_TOKEN_MINUS: {
            KalidousNode* operand = parse_expr_bp(p, 13); // prefix bp alto
            const KalidousUnaryOpData u = { KALIDOUS_TOKEN_MINUS, operand, false };
            return kalidous_ast_make_unary_op(p->arena, loc, u);
        }
        case KALIDOUS_TOKEN_NOT: {
            KalidousNode* operand = parse_expr_bp(p, 13);
            const KalidousUnaryOpData u = { KALIDOUS_TOKEN_NOT, operand, false };
            return kalidous_ast_make_unary_op(p->arena, loc, u);
        }

        // -- Agrupamento --
        case KALIDOUS_TOKEN_LPAREN: {
            KalidousNode* expr = parse_expression(p);
            expect(p, KALIDOUS_TOKEN_RPAREN, "expected ')'");
            // TODO: distinguir grouping de tuple literal (a, b, c)
            return expr;
        }

        // -- Spawn expression --
        case KALIDOUS_TOKEN_SPAWN: {
            KalidousNode* expr = parse_expression(p);
            // TODO: criar KALIDOUS_NODE_SPAWN_EXPR
            return expr;
        }

        default:
            fprintf(stderr, "[parse error] line %zu: unexpected token in expression (type %d)\n",
                    loc.line, static_cast<int>(t->type));
            p->had_error = true;
            return kalidous_ast_make_error(p->arena, loc, "unexpected token");
    }
}

static KalidousNode* parse_expr_bp(Parser* p, const int min_bp) {
    KalidousNode* left = parse_nud(p);

    while (true) {
        const KalidousTokenType op = peek(p)->type;
        BindingPower bp = infix_bp(op);
        if (bp.left < min_bp) break;

        KalidousSourceLoc loc = peek(p)->loc;
        advance(p); // consome o operador

        // -- Acesso a membro: a.b --
        if (op == KALIDOUS_TOKEN_DOT) {
            const KalidousToken* member = expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                                 "expected member name after '.'");
            KalidousNode* rhs = kalidous_ast_make_identifier(p->arena, member->loc,
                                    member->lexeme.data, member->lexeme.len);
            // TODO: criar KALIDOUS_NODE_MEMBER com left + rhs
            // Temporário: retorna rhs até o nó existir
            left = rhs;
            continue;
        }

        // -- Encadeamento: a -> b(...) --
        if (op == KALIDOUS_TOKEN_ARROW) {
            // TODO: parsear chamada encadeada (right side deve ser call)
            KalidousNode* rhs = parse_expr_bp(p, bp.right);
            // TODO: criar KALIDOUS_NODE_ARROW_CALL
            left = rhs;
            continue;
        }

        // -- Binário normal --
        KalidousNode* right = parse_expr_bp(p, bp.right);
        const KalidousBinaryOpData bin = { op, left, right };
        left = kalidous_ast_make_binary_op(p->arena, loc, bin);
    }

    // Sufixos postfix: ?, !, as Type
    while (true) {
        const KalidousSourceLoc loc = peek(p)->loc;
        if (match(p, KALIDOUS_TOKEN_QUESTION)) {
            const KalidousUnaryOpData u = { KALIDOUS_TOKEN_QUESTION, left, true };
            left = kalidous_ast_make_unary_op(p->arena, loc, u);
        } else if (match(p, KALIDOUS_TOKEN_BANG)) {
            const KalidousUnaryOpData u = { KALIDOUS_TOKEN_BANG, left, true };
            left = kalidous_ast_make_unary_op(p->arena, loc, u);
        } else {
            // TODO: 'as' cast (precisa de keyword própria ou reutilizar IDENTIFIER "as")
            break;
        }
    }

    return left;
}

static KalidousNode* parse_expression(Parser* p) {
    return parse_expr_bp(p, 0);
}

// ============================================================================
// Statements
// ============================================================================

static KalidousNode* parse_var_decl(Parser* p, bool is_const) {
    const KalidousSourceLoc loc = peek(p)->loc;

    bool is_mutable = false;

    // let / var / const / auto / mut
    // TODO: distinguir semânticas: let = imutável, var = mutável, const = comptime
    if (match(p, KALIDOUS_TOKEN_MUTABLE)) is_mutable = true;

    const KalidousToken* name = expect(p, KALIDOUS_TOKEN_IDENTIFIER, "expected variable name");

    KalidousNode* type_node = nullptr;
    if (match(p, KALIDOUS_TOKEN_COLON)) {
        type_node = parse_type(p);
    }

    KalidousNode* initializer = nullptr;
    if (match(p, KALIDOUS_TOKEN_ASSIGNMENT) || match(p, KALIDOUS_TOKEN_DECLARATION)) {
        initializer = parse_expression(p);
    }

    expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after variable declaration");

    const KalidousVarDeclData decl = {
        name->lexeme.data, name->lexeme.len,
        type_node, initializer,
        is_mutable, is_const,
        false, false
    };
    return kalidous_ast_make_var_decl(p->arena, loc, decl);
}

static KalidousNode* parse_if(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'if'

    KalidousNode* condition = parse_expression(p);
    KalidousNode* then_branch = parse_block(p);
    KalidousNode* else_branch = nullptr;

    if (match(p, KALIDOUS_TOKEN_ELSE)) {
        if (check(p, KALIDOUS_TOKEN_IF))
            else_branch = parse_if(p);      // else if
        else
            else_branch = parse_block(p);   // else { }
    }

    const KalidousIfData data = { condition, then_branch, else_branch };
    return kalidous_ast_make_if(p->arena, loc, data);
}

static KalidousNode* parse_for(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'for'

    // TODO: distinguir:
    //   for x in collection { }          → is_for_in = true
    //   for init; cond; step { }         → C-style
    //   for condition { }                → while-style
    // Por agora parseia apenas "for x in collection"

    KalidousForData data = {};
    data.is_for_in = true;

    data.iterator_var = parse_expression(p); // TODO: deve ser apenas um identificador
    expect(p, KALIDOUS_TOKEN_IN, "expected 'in' after for variable");
    data.iterable = parse_expression(p);
    data.body = parse_block(p);

    return kalidous_ast_make_for(p->arena, loc, data);
}

static KalidousNode* parse_switch(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'switch'

    // TODO: parsear switch/match completo
    //   switch expr {
    //       case pattern: stmt*
    //       case pattern if guard: stmt*
    //       _: stmt*   (default)
    //   }
    // TODO: pattern matching sobre tipos (destructuring)
    // TODO: exhaustiveness check na sema

    KalidousNode* subject = parse_expression(p);
    expect(p, KALIDOUS_TOKEN_LBRACE, "expected '{' after switch expression");
    expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}'"); // placeholder

    const KalidousSwitchData data = { subject, nullptr, 0, nullptr };
    return kalidous_ast_make_switch(p->arena, loc, data);
}

static KalidousNode* parse_try_catch(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'try'

    KalidousNode* try_block = parse_block(p);

    KalidousTryCatchData data = { try_block, nullptr, 0, nullptr };

    if (match(p, KALIDOUS_TOKEN_CATCH)) {
        // TODO: parsear "catch e" ou "catch (e: ErrorType)"
        if (check(p, KALIDOUS_TOKEN_IDENTIFIER)) {
            const KalidousToken* evar = advance(p);
            data.catch_var     = evar->lexeme.data;
            data.catch_var_len = evar->lexeme.len;
        }
        data.catch_block = parse_block(p);
    }

    return kalidous_ast_make_try_catch(p->arena, loc, data);
}

static KalidousNode* parse_return(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'return'

    KalidousNode* value = nullptr;
    if (!check(p, KALIDOUS_TOKEN_SEMICOLON) && !is_at_end(p)) {
        value = parse_expression(p);
    }
    expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after return");
    return kalidous_ast_make_return(p->arena, loc, value);
}

static KalidousNode* parse_block(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    expect(p, KALIDOUS_TOKEN_LBRACE, "expected '{'");

    // TODO: allocar array dinâmico de statements na arena (linked list ou pre-alloc)
    // Por agora retorna bloco vazio enquanto o array builder não existe
    KalidousNode** stmts = nullptr;
    size_t count = 0;

    while (!check(p, KALIDOUS_TOKEN_RBRACE) && !is_at_end(p)) {
        // TODO: push parse_statement(p) no array de stmts
        const KalidousNode* stmt = parse_statement(p);
        (void)stmt; // TODO: remover quando o array estiver pronto
        count++;
    }

    expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}'");
    return kalidous_ast_make_block(p->arena, loc, stmts, count);
}

static KalidousNode* parse_statement(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;

    switch (peek(p)->type) {
        case KALIDOUS_TOKEN_LET:
        case KALIDOUS_TOKEN_VAR:
        case KALIDOUS_TOKEN_AUTO:
            advance(p);
            return parse_var_decl(p, false);

        case KALIDOUS_TOKEN_CONST:
            advance(p);
            return parse_var_decl(p, true);

        case KALIDOUS_TOKEN_IF:      return parse_if(p);
        case KALIDOUS_TOKEN_FOR:     return parse_for(p);
        case KALIDOUS_TOKEN_SWITCH:  return parse_switch(p);
        case KALIDOUS_TOKEN_TRY:     return parse_try_catch(p);
        case KALIDOUS_TOKEN_RETURN:  return parse_return(p);

        case KALIDOUS_TOKEN_BREAK:
            advance(p);
            expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after break");
            return kalidous_ast_make_return(p->arena, loc, nullptr); // TODO: BREAK node

        case KALIDOUS_TOKEN_CONTINUE:
            advance(p);
            expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after continue");
            return kalidous_ast_make_return(p->arena, loc, nullptr); // TODO: CONTINUE node

        case KALIDOUS_TOKEN_GOTO: {
            advance(p);
            // TODO: parsear label identifier
            expect(p, KALIDOUS_TOKEN_IDENTIFIER, "expected label after goto");
            expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after goto");
            return kalidous_ast_make_error(p->arena, loc, "goto TODO"); // TODO: GOTO node
        }

        case KALIDOUS_TOKEN_MARKER: {
            advance(p);
            // TODO: parsear "marker label:"
            expect(p, KALIDOUS_TOKEN_IDENTIFIER, "expected label name after marker");
            expect(p, KALIDOUS_TOKEN_COLON, "expected ':' after marker label");
            return kalidous_ast_make_error(p->arena, loc, "marker TODO"); // TODO: MARKER node
        }

        case KALIDOUS_TOKEN_SPAWN: {
            advance(p);
            // TODO: distinguir spawn expr vs spawn { block }
            KalidousNode* body = parse_block(p);
            return body; // TODO: SPAWN_STMT node
        }

        case KALIDOUS_TOKEN_AWAIT: {
            advance(p);
            KalidousNode* expr = parse_expression(p);
            expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after await");
            return expr; // TODO: AWAIT_STMT node
        }

        default: {
            // expression statement: expr ;
            KalidousNode* expr = parse_expression(p);

            // atribuição: a = b  ou  a := b  ou  a += b, etc.
            if (const KalidousTokenType op = peek(p)->type; op == KALIDOUS_TOKEN_ASSIGNMENT  ||
                                                      op == KALIDOUS_TOKEN_DECLARATION ||
                                                      op == KALIDOUS_TOKEN_PLUS_EQUAL  ||
                                                      op == KALIDOUS_TOKEN_MINUS_EQUAL ||
                                                      op == KALIDOUS_TOKEN_MULTIPLY_EQUAL ||
                                                      op == KALIDOUS_TOKEN_DIVIDE_EQUAL) {

                const KalidousSourceLoc assign_loc = peek(p)->loc;
                advance(p);
                KalidousNode* value = parse_expression(p);
                const KalidousBinaryOpData bin = { op, expr, value };
                expr = kalidous_ast_make_binary_op(p->arena, assign_loc, bin);
            }

            expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after expression");
            return expr;
        }
    }
}

// ============================================================================
// Declarações de topo
// ============================================================================

static KalidousNode* parse_func_decl(Parser* p, const bool is_public) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'fn' ou keyword equivalente

    const KalidousToken* name = expect(p, KALIDOUS_TOKEN_IDENTIFIER, "expected function name");
    expect(p, KALIDOUS_TOKEN_LPAREN, "expected '(' after function name");

    // TODO: parsear lista de parâmetros completa
    //   - parâmetros com tipo, default value, ownership modifier
    //   - parâmetros variádicos (...args)
    //   - self / this para métodos
    KalidousNode** params = nullptr;
    size_t param_count = 0;

    while (!check(p, KALIDOUS_TOKEN_RPAREN) && !is_at_end(p)) {
        // TODO: push parse_param(p) no array
        // Placeholder: consome tokens até encontrar ',' ou ')'
        advance(p);
        match(p, KALIDOUS_TOKEN_COMMA);
        param_count++;
    }
    expect(p, KALIDOUS_TOKEN_RPAREN, "expected ')' after parameters");

    KalidousNode* return_type = nullptr;
    if (match(p, KALIDOUS_TOKEN_ARROW)) {
        return_type = parse_type(p);
    }

    KalidousNode* body = nullptr;
    if (check(p, KALIDOUS_TOKEN_LBRACE))
        body = parse_block(p);
    else
        expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' for forward declaration");

    // TODO: generics:  fn foo<T: Trait>(...)
    // TODO: where clause: fn foo<T>(...) where T: Trait
    // TODO: extern fn para FFI
    // TODO: atributos: #[inline], #[cold], #[no_mangle]

    const KalidousFuncDeclData decl = {
        name->lexeme.data, name->lexeme.len,
        params, param_count,
        return_type, body,
        is_public, false
    };
    return kalidous_ast_make_func_decl(p->arena, loc, decl);
}

static KalidousNode* parse_struct_decl(Parser* p, const bool is_public) {
    KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'struct'

    const KalidousToken* name = expect(p, KALIDOUS_TOKEN_IDENTIFIER, "expected struct name");
    expect(p, KALIDOUS_TOKEN_LBRACE, "expected '{'");

    // TODO: parsear campos:  name: Type [= default]
    // TODO: parsear métodos inline (fn dentro do struct)
    // TODO: atributos de campo: #[packed], #[align(N)], visibilidade

    KalidousNode** fields  = nullptr;
    size_t         field_count  = 0;
    KalidousNode** methods = nullptr;
    size_t         method_count = 0;

    while (!check(p, KALIDOUS_TOKEN_RBRACE) && !is_at_end(p)) {
        // TODO: distinguir field vs inline fn
        advance(p); // placeholder
    }
    expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}'");

    KalidousStructDeclData decl = {
        name->lexeme.data, name->lexeme.len,
        fields, field_count,
        methods, method_count,
        is_public
    };
    return kalidous_ast_make_struct(p->arena, loc, decl);
}

static KalidousNode* parse_enum_decl(Parser* p, bool is_public) {
    KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'enum'

    const KalidousToken* name = expect(p, KALIDOUS_TOKEN_IDENTIFIER, "expected enum name");
    expect(p, KALIDOUS_TOKEN_LBRACE, "expected '{'");

    // TODO: parsear variantes: Name [= value] [{ fields }]
    // TODO: family = variantes com payload (tagged union)
    // TODO: backing type: enum Foo: u8 { ... }

    KalidousNode** variants = nullptr;
    size_t variant_count = 0;

    while (!check(p, KALIDOUS_TOKEN_RBRACE) && !is_at_end(p)) {
        advance(p); // placeholder
        match(p, KALIDOUS_TOKEN_COMMA);
        variant_count++;
    }
    expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}'");

    KalidousEnumDeclData decl = {
        name->lexeme.data, name->lexeme.len,
        variants, variant_count,
        is_public
    };
    return kalidous_ast_make_enum(p->arena, loc, decl);
}

static KalidousNode* parse_import(Parser* p) {
    KalidousSourceLoc loc = peek(p)->loc;
    // TODO: keyword 'use' precisa de token próprio ou reusar IDENTIFIER "use"
    advance(p);

    // TODO: parsear path completo: use std::io::File
    // TODO: aliases: use std::io::File as F
    // TODO: wildcards: use std::io::*
    // TODO: selective: use std::io::{Read, Write}

    KalidousImportData data = { nullptr, 0, nullptr, false };
    expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after import");
    return kalidous_ast_make_import(p->arena, loc, data);
}

static KalidousNode* parse_declaration(Parser* p) {
    bool is_public = false;

    // modificador de acesso: public / private / protected
    if (check(p, KALIDOUS_TOKEN_MODIFIER)) {
        // TODO: verificar lexeme para distinguir public/private/protected
        advance(p);
        is_public = true;
    }

    KalidousTokenType t = peek(p)->type;

    // TODO: keyword 'fn' — por agora reutiliza IDENTIFIER "fn" ou outro token?
    //       Provavelmente precisa de KALIDOUS_TOKEN_FN dedicado no tokenizer
    if (t == KALIDOUS_TOKEN_STRUCT)    return parse_struct_decl(p, is_public);
    if (t == KALIDOUS_TOKEN_ENUM)      return parse_enum_decl(p, is_public);
    if (t == KALIDOUS_TOKEN_CONST)     { advance(p); return parse_var_decl(p, true); }
    if (t == KALIDOUS_TOKEN_LET ||
        t == KALIDOUS_TOKEN_VAR)       { advance(p); return parse_var_decl(p, false); }

    // TODO: COMPONENT, UNION, FAMILY, ENTITY, TRAIT, IMPLEMENT, MODULE
    // TODO: 'use' / import

    // Fallback: expression statement no topo (erro provável)
    fprintf(stderr, "[parse error] line %zu: unexpected token at top level (type %d)\n",
            peek(p)->loc.line, (int)t);
    p->had_error = true;
    KalidousNode* expr = parse_expression(p);
    match(p, KALIDOUS_TOKEN_SEMICOLON);
    return expr;
}

// ============================================================================
// Entry point — implementa kalidous_parse declarado em kalidous.h
// ============================================================================

KalidousNode* kalidous_parse(KalidousArena* arena, KalidousTokenStream tokens) {
    Parser p = { arena, tokens.data, tokens.len, 0, false };

    // TODO: pre-alocar array de top-level decls na arena (lista ligada ou vector)
    KalidousNode** decls = nullptr;
    size_t count = 0;

    while (!is_at_end(&p)) {
        KalidousNode* decl = parse_declaration(&p);
        (void)decl; // TODO: push no array
        count++;

        // TODO: error recovery — se had_error, tentar sincronizar e continuar
    }

    KalidousSourceLoc root_loc = { 0, 1 };
    return kalidous_ast_make_program(arena, decls, count);
}