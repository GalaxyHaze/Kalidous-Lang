// kalidous_parser.cpp — Recursive descent parser for Kalidous
#include <Kalidous/kalidous.h>
#include "ast.h"
#include <cstdio>
#include <cstring>

// NOTE: este ficheiro assume que KALIDOUS_TOKEN_FN foi adicionado a kalidous.h
//       e ao TokenTable em keywords.cpp.  Sem ele, "fn" é tokenizado como
//       IDENTIFIER e parse_declaration nunca reconhece funções.

// ============================================================================
// Parser state
// ============================================================================

struct Parser {
    KalidousArena*       arena;
    const KalidousToken* tokens;
    size_t               count;
    size_t               pos;
    bool                 had_error;

    // TODO: acumular múltiplos erros antes de abortar
    // TODO: error spans com linha:coluna de início e fim
    // TODO: níveis: error / warning / note / hint
    // TODO: sugestões de fix ("did you mean X?")
};

// ============================================================================
// Arena array builder — coleta nós durante parsing
// ============================================================================
//
// Usa uma lista ligada de blocos alocados na arena; flatten() no final
// produz o array contíguo que os payloads da AST esperam.

struct NodeBuilder {
    struct Chunk {
        KalidousNode* items[16];
        size_t        len;
        Chunk*        next;
    };

    Chunk*  head   = nullptr;
    Chunk*  tail   = nullptr;
    size_t  total  = 0;

    void push(KalidousArena* arena, KalidousNode* n) {
        if (!tail || tail->len == 16) {
            auto* c = static_cast<Chunk*>(kalidous_arena_alloc(arena, sizeof(Chunk)));
            *c = {};
            if (tail) tail->next = c;
            else      head       = c;
            tail = c;
        }
        tail->items[tail->len++] = n;
        total++;
    }

    // Retorna array contíguo alocado na arena (ou nullptr se vazio)
    KalidousNode** flatten(KalidousArena* arena, size_t* out_count) const {
        *out_count = total;
        if (total == 0) return nullptr;
        auto** arr = static_cast<KalidousNode**>(
            kalidous_arena_alloc(arena, total * sizeof(KalidousNode*)));
        size_t i = 0;
        for (const Chunk* c = head; c; c = c->next)
            for (size_t j = 0; j < c->len; ++j)
                arr[i++] = c->items[j];
        return arr;
    }
};

// ============================================================================
// Helpers básicos
// ============================================================================

static const KalidousToken* peek(const Parser* p) {
    if (p->pos < p->count) return &p->tokens[p->pos];
    static constexpr KalidousToken eof = { {nullptr, 0}, {0, 0}, KALIDOUS_TOKEN_END, 0 };
    return &eof;
}

static const KalidousToken* peek_ahead(const Parser* p, size_t offset) {
    const size_t idx = p->pos + offset;
    if (idx < p->count) return &p->tokens[idx];
    static constexpr KalidousToken eof = { {nullptr, 0}, {0, 0}, KALIDOUS_TOKEN_END, 0 };
    return &eof;
}

static const KalidousToken* advance(Parser* p) {
    const KalidousToken* t = peek(p);
    if (p->pos < p->count) p->pos++;
    return t;
}

static bool check(const Parser* p, KalidousTokenType type) {
    return peek(p)->type == type;
}

static bool match(Parser* p, KalidousTokenType type) {
    if (check(p, type)) { advance(p); return true; }
    return false;
}

static const KalidousToken* expect(Parser* p, KalidousTokenType type, const char* msg) {
    if (check(p, type)) return advance(p);
    const KalidousToken* t = peek(p);
    fprintf(stderr, "[parse error] line %zu: %s (got token type %d)\n",
            t->loc.line, msg, static_cast<int>(t->type));
    p->had_error = true;
    // TODO: error recovery — skip até synchronization point (';', '}', keyword de topo)
    return t;
}

static bool is_at_end(const Parser* p) {
    return peek(p)->type == KALIDOUS_TOKEN_END;
}

// Verifica se o token atual é um IDENTIFIER com lexeme específico
// Útil para keywords que ainda não têm token próprio
static bool check_kw(const Parser* p, const char* kw) {
    const KalidousToken* t = peek(p);
    if (t->type != KALIDOUS_TOKEN_IDENTIFIER) return false;
    const size_t len = strlen(kw);
    return t->lexeme.len == len && memcmp(t->lexeme.data, kw, len) == 0;
}

// ============================================================================
// Forward declarations
// ============================================================================

static KalidousNode* parse_declaration(Parser* p);
static KalidousNode* parse_statement  (Parser* p);
static KalidousNode* parse_expression (Parser* p);
static KalidousNode* parse_type       (Parser* p);
static KalidousNode* parse_block      (Parser* p);

// ============================================================================
// Tipos
// ============================================================================

static KalidousNode* parse_type(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;

    // Ownership modifiers como prefixo de tipo
    KalidousOwnership own = KALIDOUS_OWN_DEFAULT;
    if      (match(p, KALIDOUS_TOKEN_UNIQUE)) own = KALIDOUS_OWN_UNIQUE;
    else if (match(p, KALIDOUS_TOKEN_SHARED)) own = KALIDOUS_OWN_SHARED;
    else if (match(p, KALIDOUS_TOKEN_VIEW))   own = KALIDOUS_OWN_VIEW;
    else if (match(p, KALIDOUS_TOKEN_LEND))   own = KALIDOUS_OWN_LEND;
    (void)own; // será usado quando TYPE_WRAPPER node existir

    // Ponteiro raw: *Type
    if (match(p, KALIDOUS_TOKEN_MULTIPLY)) {
        KalidousNode* inner = parse_type(p);
        // TODO: KALIDOUS_NODE_TYPE_POINTER com inner
        return inner;
    }

    // Array: [N]Type ou []Type
    if (match(p, KALIDOUS_TOKEN_LBRACKET)) {
        KalidousNode* size_expr = nullptr;
        if (!check(p, KALIDOUS_TOKEN_RBRACKET))
            size_expr = parse_expression(p);
        expect(p, KALIDOUS_TOKEN_RBRACKET, "expected ']' in array type");
        KalidousNode* inner = parse_type(p);
        (void)size_expr;
        // TODO: KALIDOUS_NODE_TYPE_ARRAY com size_expr + inner
        return inner;
    }

    // Tipo nomeado (inclui primitivos: i32, u64, bool, ...)
    if (check(p, KALIDOUS_TOKEN_TYPE) || check(p, KALIDOUS_TOKEN_IDENTIFIER)) {
        const KalidousToken* t = advance(p);
        KalidousNode* base = kalidous_ast_make_identifier(p->arena, loc,
                                                          t->lexeme.data, t->lexeme.len);
        // Sufixo opcional / result: Type? ou Type!
        if (match(p, KALIDOUS_TOKEN_QUESTION)) {
            // TODO: KALIDOUS_NODE_TYPE_OPTIONAL wrapping base
        } else if (match(p, KALIDOUS_TOKEN_BANG)) {
            // TODO: KALIDOUS_NODE_TYPE_RESULT wrapping base
        }
        return base;
    }

    fprintf(stderr, "[parse error] line %zu: expected type\n", loc.line);
    p->had_error = true;
    return kalidous_ast_make_error(p->arena, loc, "expected type");
}

// ============================================================================
// Expressões (Pratt parser)
// ============================================================================

typedef struct { int left; int right; } BindingPower;

static BindingPower infix_bp(KalidousTokenType op) {
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
        case KALIDOUS_TOKEN_ARROW:                 return {13, 14};
        case KALIDOUS_TOKEN_DOT:                   return {15, 16};
        default:                                   return {-1, -1};
    }
}

static KalidousNode* parse_expr_bp(Parser* p, int min_bp);

static KalidousNode* parse_nud(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    const KalidousToken* t = advance(p);

    switch (t->type) {

        // -- Literais escalares -----------------------------------------------
        case KALIDOUS_TOKEN_NUMBER: {
            KalidousLiteralData lit = {};
            lit.kind      = KALIDOUS_TOKEN_NUMBER;
            lit.value.i64 = 0; // TODO: strtoll(t->lexeme.data)
            return kalidous_ast_make_literal(p->arena, loc, lit);
        }
        case KALIDOUS_TOKEN_FLOAT: {
            KalidousLiteralData lit = {};
            lit.kind      = KALIDOUS_TOKEN_FLOAT;
            lit.value.f64 = 0.0; // TODO: strtod(t->lexeme.data)
            return kalidous_ast_make_literal(p->arena, loc, lit);
        }
        case KALIDOUS_TOKEN_STRING: {
            KalidousLiteralData lit = {};
            lit.kind               = KALIDOUS_TOKEN_STRING;
            lit.value.string       = { t->lexeme.data, t->lexeme.len };
            return kalidous_ast_make_literal(p->arena, loc, lit);
        }
        case KALIDOUS_TOKEN_HEXADECIMAL:
        case KALIDOUS_TOKEN_OCTAL:
        case KALIDOUS_TOKEN_BINARY: {
            KalidousLiteralData lit = {};
            lit.kind      = t->type;
            lit.value.u64 = 0; // TODO: converter base do lexeme
            return kalidous_ast_make_literal(p->arena, loc, lit);
        }

        // -- Identificador / chamada ------------------------------------------
        case KALIDOUS_TOKEN_IDENTIFIER: {
            KalidousNode* ident = kalidous_ast_make_identifier(p->arena, loc,
                                      t->lexeme.data, t->lexeme.len);
            if (!check(p, KALIDOUS_TOKEN_LPAREN)) return ident;

            advance(p); // consome '('
            NodeBuilder args_b;
            while (!check(p, KALIDOUS_TOKEN_RPAREN) && !is_at_end(p)) {
                args_b.push(p->arena, parse_expression(p));
                if (!match(p, KALIDOUS_TOKEN_COMMA)) break;
            }
            expect(p, KALIDOUS_TOKEN_RPAREN, "expected ')' after arguments");

            size_t         arg_count = 0;
            KalidousNode** args      = args_b.flatten(p->arena, &arg_count);
            const KalidousCallData call = { ident, args, arg_count };
            return kalidous_ast_make_call(p->arena, loc, call);
        }

        // -- recurse(...) — CPS transform pelo backend -----------------------
        // NOTE: recurse é uma keyword; se KALIDOUS_TOKEN_RECURSE não existir ainda,
        //       pode ser detectado como IDENTIFIER via check_kw antes de advance()
        case KALIDOUS_TOKEN_RECURSE: {
            // Reutiliza o mesmo parsing de chamada, emite NODE_RECURSE
            // A sema valida que só ocorre dentro de fn com recurse
            KalidousNode* self_ref = kalidous_ast_make_identifier(p->arena, loc,
                                         t->lexeme.data, t->lexeme.len);
            expect(p, KALIDOUS_TOKEN_LPAREN, "expected '(' after recurse");
            NodeBuilder args_b;
            while (!check(p, KALIDOUS_TOKEN_RPAREN) && !is_at_end(p)) {
                args_b.push(p->arena, parse_expression(p));
                if (!match(p, KALIDOUS_TOKEN_COMMA)) break;
            }
            expect(p, KALIDOUS_TOKEN_RPAREN, "expected ')' after recurse arguments");

            size_t         arg_count = 0;
            KalidousNode** args      = args_b.flatten(p->arena, &arg_count);
            const KalidousCallData call = { self_ref, args, arg_count };
            return kalidous_ast_make_recurse(p->arena, loc, call);
        }

        // -- Unários prefixo --------------------------------------------------
        case KALIDOUS_TOKEN_MINUS: {
            KalidousNode* operand = parse_expr_bp(p, 13);
            const KalidousUnaryOpData u = { KALIDOUS_TOKEN_MINUS, operand, false };
            return kalidous_ast_make_unary_op(p->arena, loc, u);
        }
        case KALIDOUS_TOKEN_BANG: {
            // NOTE: "not" está mapeado para NOT_EQUAL (operador binário infixo).
            //       Negação lógica unária usa '!' explícito.
            //       Se for necessário "not" unário, precisará de token próprio.
            KalidousNode* operand = parse_expr_bp(p, 13);
            const KalidousUnaryOpData u = { KALIDOUS_TOKEN_BANG, operand, false };
            return kalidous_ast_make_unary_op(p->arena, loc, u);
        }

        // -- Agrupamento / tuple ----------------------------------------------
        case KALIDOUS_TOKEN_LPAREN: {
            KalidousNode* expr = parse_expression(p);
            // TODO: distinguir grouping de tuple (a, b, c)
            expect(p, KALIDOUS_TOKEN_RPAREN, "expected ')'");
            return expr;
        }

        // -- Spawn expression ------------------------------------------------
        case KALIDOUS_TOKEN_SPAWN: {
            KalidousNode* expr = parse_expression(p);
            // TODO: KALIDOUS_NODE_SPAWN_EXPR wrapping expr
            return expr;
        }

        default:
            fprintf(stderr, "[parse error] line %zu: unexpected token in expression (type %d)\n",
                    loc.line, static_cast<int>(t->type));
            p->had_error = true;
            return kalidous_ast_make_error(p->arena, loc, "unexpected token in expression");
    }
}

static KalidousNode* parse_expr_bp(Parser* p, const int min_bp) {
    KalidousNode* left = parse_nud(p);

    // Infixo
    while (true) {
        const KalidousTokenType op = peek(p)->type;
        const BindingPower bp = infix_bp(op);
        if (bp.left < min_bp) break;

        const KalidousSourceLoc loc = peek(p)->loc;
        advance(p);

        if (op == KALIDOUS_TOKEN_DOT) {
            const KalidousToken* member = expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                                 "expected member name after '.'");
            KalidousNode* rhs = kalidous_ast_make_identifier(p->arena, member->loc,
                                    member->lexeme.data, member->lexeme.len);
            // TODO: KALIDOUS_NODE_MEMBER_ACCESS { left, rhs }
            left = rhs;
            continue;
        }

        if (op == KALIDOUS_TOKEN_ARROW) {
            // Encadeamento: a->b(...)  — right deve ser call
            KalidousNode* rhs = parse_expr_bp(p, bp.right);
            // TODO: KALIDOUS_NODE_ARROW_CALL { left, rhs }
            (void)loc;
            left = rhs;
            continue;
        }

        KalidousNode* right = parse_expr_bp(p, bp.right);
        const KalidousBinaryOpData bin = { op, left, right };
        left = kalidous_ast_make_binary_op(p->arena, loc, bin);
    }

    // Sufixos postfix: ?, !
    while (true) {
        const KalidousSourceLoc loc = peek(p)->loc;
        if (match(p, KALIDOUS_TOKEN_QUESTION)) {
            const KalidousUnaryOpData u = { KALIDOUS_TOKEN_QUESTION, left, true };
            left = kalidous_ast_make_unary_op(p->arena, loc, u);
        } else if (match(p, KALIDOUS_TOKEN_BANG)) {
            const KalidousUnaryOpData u = { KALIDOUS_TOKEN_BANG, left, true };
            left = kalidous_ast_make_unary_op(p->arena, loc, u);
        } else {
            // TODO: 'as' cast — precisa de KALIDOUS_TOKEN_AS ou check_kw("as")
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

static KalidousNode* parse_var_decl(Parser* p, KalidousBindingKind binding) {
    const KalidousSourceLoc loc = peek(p)->loc;

    // Ownership modifier opcional após o binding keyword
    // ex: var unique x: Foo = ...
    KalidousOwnership own = KALIDOUS_OWN_DEFAULT;
    if      (match(p, KALIDOUS_TOKEN_UNIQUE)) own = KALIDOUS_OWN_UNIQUE;
    else if (match(p, KALIDOUS_TOKEN_SHARED)) own = KALIDOUS_OWN_SHARED;
    else if (match(p, KALIDOUS_TOKEN_VIEW))   own = KALIDOUS_OWN_VIEW;
    else if (match(p, KALIDOUS_TOKEN_LEND))   own = KALIDOUS_OWN_LEND;

    const KalidousToken* name = expect(p, KALIDOUS_TOKEN_IDENTIFIER, "expected variable name");

    KalidousNode* type_node = nullptr;
    if (match(p, KALIDOUS_TOKEN_COLON))
        type_node = parse_type(p);

    KalidousNode* initializer = nullptr;
    if (match(p, KALIDOUS_TOKEN_ASSIGNMENT) || match(p, KALIDOUS_TOKEN_DECLARATION))
        initializer = parse_expression(p);

    expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after variable declaration");

    const KalidousVarDeclData decl = {
        name->lexeme.data, name->lexeme.len,
        binding, own,
        type_node, initializer
    };
    return kalidous_ast_make_var_decl(p->arena, loc, decl);
}

static KalidousNode* parse_if(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'if'

    KalidousNode* condition   = parse_expression(p);
    KalidousNode* then_branch = parse_block(p);
    KalidousNode* else_branch = nullptr;

    if (match(p, KALIDOUS_TOKEN_ELSE)) {
        else_branch = check(p, KALIDOUS_TOKEN_IF)
            ? parse_if(p)
            : parse_block(p);
    }

    const KalidousIfData data = { condition, then_branch, else_branch };
    return kalidous_ast_make_if(p->arena, loc, data);
}

static KalidousNode* parse_for(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'for'

    KalidousForData data = {};

    // for x in collection { }
    // Heurística: se o token depois do identificador é 'in', é for-in
    if (check(p, KALIDOUS_TOKEN_IDENTIFIER) &&
        peek_ahead(p, 1)->type == KALIDOUS_TOKEN_IN) {

        data.is_for_in    = true;
        data.iterator_var = parse_expression(p); // apenas identif.
        advance(p);                               // consome 'in'
        data.iterable     = parse_expression(p);
        data.body         = parse_block(p);
        return kalidous_ast_make_for(p->arena, loc, data);
    }

    // for cond { }  —  while-style
    // TODO: for init; cond; step { }  — C-style (requer ';' lookahead)
    data.is_for_in  = false;
    data.condition  = parse_expression(p);
    data.body       = parse_block(p);
    return kalidous_ast_make_for(p->arena, loc, data);
}

static KalidousNode* parse_switch(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'switch'

    KalidousNode* subject = parse_expression(p);
    expect(p, KALIDOUS_TOKEN_LBRACE, "expected '{' after switch expression");

    NodeBuilder arms_b;
    KalidousNode* default_arm = nullptr;

    // TODO: parsear case pattern [if guard]: stmt*
    // TODO: pattern matching com destructuring
    // TODO: exhaustiveness check na sema
    while (!check(p, KALIDOUS_TOKEN_RBRACE) && !is_at_end(p)) {
        advance(p); // placeholder
    }

    expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}'");

    size_t         arm_count = 0;
    KalidousNode** arms      = arms_b.flatten(p->arena, &arm_count);

    const KalidousSwitchData data = { subject, arms, arm_count, default_arm };
    return kalidous_ast_make_switch(p->arena, loc, data);
}

static KalidousNode* parse_try_catch(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'try'

    KalidousNode* try_block = parse_block(p);
    KalidousTryCatchData data = { try_block, nullptr, 0, nullptr };

    if (match(p, KALIDOUS_TOKEN_CATCH)) {
        if (check(p, KALIDOUS_TOKEN_IDENTIFIER)) {
            const KalidousToken* evar = advance(p);
            data.catch_var     = evar->lexeme.data;
            data.catch_var_len = evar->lexeme.len;
        }
        // TODO: catch e: ErrorType  — tipagem do erro
        // TODO: múltiplos catch por tipo
        // TODO: finally block
        data.catch_block = parse_block(p);
    }

    return kalidous_ast_make_try_catch(p->arena, loc, data);
}

static KalidousNode* parse_return(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'return'

    KalidousNode* value = nullptr;
    if (!check(p, KALIDOUS_TOKEN_SEMICOLON) && !is_at_end(p))
        value = parse_expression(p);

    expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after return");
    return kalidous_ast_make_return(p->arena, loc, value);
}

static KalidousNode* parse_yield(Parser* p) {
    // Válido apenas em async fn — a sema verifica o contexto
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'yield'

    KalidousNode* value = nullptr;
    if (!check(p, KALIDOUS_TOKEN_SEMICOLON) && !is_at_end(p))
        value = parse_expression(p);

    expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after yield");
    return kalidous_ast_make_yield(p->arena, loc, value);
}

static KalidousNode* parse_block(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;
    expect(p, KALIDOUS_TOKEN_LBRACE, "expected '{'");

    NodeBuilder stmts_b;
    while (!check(p, KALIDOUS_TOKEN_RBRACE) && !is_at_end(p))
        stmts_b.push(p->arena, parse_statement(p));

    expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}'");

    size_t         count = 0;
    KalidousNode** stmts = stmts_b.flatten(p->arena, &count);
    return kalidous_ast_make_block(p->arena, loc, stmts, count);
}

static KalidousNode* parse_statement(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;

    switch (peek(p)->type) {

        case KALIDOUS_TOKEN_LET:    advance(p); return parse_var_decl(p, KALIDOUS_BINDING_LET);
        case KALIDOUS_TOKEN_VAR:    advance(p); return parse_var_decl(p, KALIDOUS_BINDING_VAR);
        case KALIDOUS_TOKEN_AUTO:   advance(p); return parse_var_decl(p, KALIDOUS_BINDING_AUTO);
        case KALIDOUS_TOKEN_CONST:  advance(p); return parse_var_decl(p, KALIDOUS_BINDING_CONST);

        case KALIDOUS_TOKEN_IF:     return parse_if(p);
        case KALIDOUS_TOKEN_FOR:    return parse_for(p);
        case KALIDOUS_TOKEN_SWITCH: return parse_switch(p);
        case KALIDOUS_TOKEN_TRY:    return parse_try_catch(p);
        case KALIDOUS_TOKEN_RETURN: return parse_return(p);

        case KALIDOUS_TOKEN_AWAIT: {
            // yield é o mecanismo de coroutine em async fn; await é para I/O
            advance(p);
            KalidousNode* expr = parse_expression(p);
            expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after await");
            // TODO: KALIDOUS_NODE_AWAIT_STMT wrapping expr
            return expr;
        }

        // yield — exclusivo de async fn; sema valida o contexto
        case KALIDOUS_TOKEN_YIELD:
            return parse_yield(p);

        case KALIDOUS_TOKEN_BREAK:
            advance(p);
            expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after break");
            // TODO: KALIDOUS_NODE_BREAK
            return kalidous_ast_make_error(p->arena, loc, "break");

        case KALIDOUS_TOKEN_CONTINUE:
            advance(p);
            expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after continue");
            // TODO: KALIDOUS_NODE_CONTINUE
            return kalidous_ast_make_error(p->arena, loc, "continue");

        case KALIDOUS_TOKEN_GOTO: {
            advance(p);
            const KalidousToken* label = expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                                "expected label after goto");
            expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after goto");
            // TODO: KALIDOUS_NODE_GOTO { label }
            (void)label;
            return kalidous_ast_make_error(p->arena, loc, "goto");
        }

        case KALIDOUS_TOKEN_MARKER: {
            advance(p);
            const KalidousToken* label = expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                                "expected label after marker");
            expect(p, KALIDOUS_TOKEN_COLON, "expected ':' after marker label");
            // TODO: KALIDOUS_NODE_MARKER { label }
            (void)label;
            return kalidous_ast_make_error(p->arena, loc, "marker");
        }

        case KALIDOUS_TOKEN_SPAWN: {
            advance(p);
            if (check(p, KALIDOUS_TOKEN_LBRACE)) {
                KalidousNode* body = parse_block(p);
                // TODO: KALIDOUS_NODE_SPAWN_STMT { body }
                return body;
            }
            KalidousNode* expr = parse_expression(p);
            expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after spawn expression");
            // TODO: KALIDOUS_NODE_SPAWN_EXPR { expr }
            return expr;
        }

        default: {
            // Expression statement — inclui atribuição
            KalidousNode* expr = parse_expression(p);

            const KalidousTokenType op = peek(p)->type;
            if (op == KALIDOUS_TOKEN_ASSIGNMENT   ||
                op == KALIDOUS_TOKEN_DECLARATION  ||
                op == KALIDOUS_TOKEN_PLUS_EQUAL   ||
                op == KALIDOUS_TOKEN_MINUS_EQUAL  ||
                op == KALIDOUS_TOKEN_MULTIPLY_EQUAL ||
                op == KALIDOUS_TOKEN_DIVIDE_EQUAL) {

                const KalidousSourceLoc assign_loc = peek(p)->loc;
                advance(p);
                KalidousNode* value = parse_expression(p);
                const KalidousBinaryOpData bin = { op, expr, value };
                expr = kalidous_ast_make_binary_op(p->arena, assign_loc, bin);
            }

            expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after expression statement");
            return expr;
        }
    }
}

// ============================================================================
// Declarações de topo
// ============================================================================

static KalidousNode* parse_param(Parser* p) {
    const KalidousSourceLoc loc = peek(p)->loc;

    // Ownership modifier opcional antes do nome
    KalidousOwnership own = KALIDOUS_OWN_DEFAULT;
    bool is_mutable = false;
    if      (match(p, KALIDOUS_TOKEN_UNIQUE)) own = KALIDOUS_OWN_UNIQUE;
    else if (match(p, KALIDOUS_TOKEN_SHARED)) own = KALIDOUS_OWN_SHARED;
    else if (match(p, KALIDOUS_TOKEN_VIEW))   own = KALIDOUS_OWN_VIEW;
    else if (match(p, KALIDOUS_TOKEN_LEND))   { own = KALIDOUS_OWN_LEND; is_mutable = true; }

    const KalidousToken* name = expect(p, KALIDOUS_TOKEN_IDENTIFIER, "expected parameter name");
    expect(p, KALIDOUS_TOKEN_COLON, "expected ':' after parameter name");
    KalidousNode* type_node = parse_type(p);

    KalidousNode* default_value = nullptr;
    if (match(p, KALIDOUS_TOKEN_ASSIGNMENT))
        default_value = parse_expression(p);

    const KalidousParamData param = {
        name->lexeme.data, name->lexeme.len,
        own, type_node, default_value, is_mutable
    };
    return kalidous_ast_make_param(p->arena, loc, param);
}

// Parseia fn, async fn, noreturn fn, flowing fn
// O token(s) que identificam o kind já foram consumidos pelo chamador;
// recebe o kind resolvido e a loc do início da declaração
static KalidousNode* parse_func_body(Parser* p, KalidousFnKind kind,
                                     KalidousSourceLoc loc, bool is_public) {
    const KalidousToken* name = expect(p, KALIDOUS_TOKEN_IDENTIFIER, "expected function name");
    expect(p, KALIDOUS_TOKEN_LPAREN, "expected '(' after function name");

    NodeBuilder params_b;
    while (!check(p, KALIDOUS_TOKEN_RPAREN) && !is_at_end(p)) {
        params_b.push(p->arena, parse_param(p));
        if (!match(p, KALIDOUS_TOKEN_COMMA)) break;
    }
    expect(p, KALIDOUS_TOKEN_RPAREN, "expected ')' after parameters");

    // noreturn fn não tem tipo de retorno — fica implicitamente noreturn
    KalidousNode* return_type = nullptr;
    if (kind != KALIDOUS_FN_NORETURN && match(p, KALIDOUS_TOKEN_ARROW))
        return_type = parse_type(p);

    KalidousNode* body = nullptr;
    if (check(p, KALIDOUS_TOKEN_LBRACE))
        body = parse_block(p);
    else
        expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' for forward declaration");

    // TODO: generics: fn foo<T: Trait>(...)
    // TODO: where clause: fn foo<T>(...) where T: Trait
    // TODO: atributos: #[inline], #[cold], #[no_mangle]

    size_t         param_count = 0;
    KalidousNode** params      = params_b.flatten(p->arena, &param_count);

    const KalidousFuncDeclData decl = {
        name->lexeme.data, name->lexeme.len,
        kind, params, param_count,
        return_type, body,
        is_public, false
    };
    return kalidous_ast_make_func_decl(p->arena, loc, decl);
}

static KalidousNode* parse_struct_decl(Parser* p, bool is_public) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'struct'

    const KalidousToken* name = expect(p, KALIDOUS_TOKEN_IDENTIFIER, "expected struct name");
    expect(p, KALIDOUS_TOKEN_LBRACE, "expected '{'");

    NodeBuilder fields_b, methods_b;

    while (!check(p, KALIDOUS_TOKEN_RBRACE) && !is_at_end(p)) {
        // fn inline → método
        if (check(p, KALIDOUS_TOKEN_FN)) {
            const KalidousSourceLoc fn_loc = peek(p)->loc;
            advance(p);
            methods_b.push(p->arena, parse_func_body(p, KALIDOUS_FN_NORMAL, fn_loc, false));
            continue;
        }
        // TODO: campos: name: Type [= default]
        // TODO: visibilidade por campo
        // TODO: atributos: #[packed], #[align(N)]
        advance(p); // placeholder
    }

    expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}'");

    size_t         field_count  = 0; KalidousNode** fields  = fields_b.flatten(p->arena, &field_count);
    size_t         method_count = 0; KalidousNode** methods = methods_b.flatten(p->arena, &method_count);

    const KalidousStructDeclData decl = {
        name->lexeme.data, name->lexeme.len,
        fields, field_count, methods, method_count,
        is_public
    };
    return kalidous_ast_make_struct(p->arena, loc, decl);
}

static KalidousNode* parse_enum_decl(Parser* p, bool is_public) {
    const KalidousSourceLoc loc = peek(p)->loc;
    advance(p); // consome 'enum'

    const KalidousToken* name = expect(p, KALIDOUS_TOKEN_IDENTIFIER, "expected enum name");
    expect(p, KALIDOUS_TOKEN_LBRACE, "expected '{'");

    NodeBuilder variants_b;

    while (!check(p, KALIDOUS_TOKEN_RBRACE) && !is_at_end(p)) {
        // TODO: parsear variantes: Name [= value] [{ fields }] (family payload)
        // TODO: backing type: enum Foo: u8 { ... }
        advance(p); // placeholder
        match(p, KALIDOUS_TOKEN_COMMA);
    }
    expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}'");

    size_t         variant_count = 0;
    KalidousNode** variants      = variants_b.flatten(p->arena, &variant_count);

    const KalidousEnumDeclData decl = {
        name->lexeme.data, name->lexeme.len,
        variants, variant_count,
        is_public
    };
    return kalidous_ast_make_enum(p->arena, loc, decl);
}

static KalidousNode* parse_declaration(Parser* p) {
    // Modificador de acesso opcional
    bool is_public = false;
    if (check(p, KALIDOUS_TOKEN_MODIFIER)) {
        // TODO: distinguir public / private / protected pelo lexeme
        advance(p);
        is_public = true;
    }

    const KalidousSourceLoc loc = peek(p)->loc;
    const KalidousTokenType t   = peek(p)->type;

    // -- Structs, enums, tipos -----------------------------------------------
    if (t == KALIDOUS_TOKEN_STRUCT)    return parse_struct_decl(p, is_public);
    if (t == KALIDOUS_TOKEN_ENUM)      return parse_enum_decl(p, is_public);

    // TODO: COMPONENT, UNION, FAMILY, ENTITY, TRAIT, IMPLEMENT, MODULE
    // TODO: 'use' / import

    // -- Bindings de topo ----------------------------------------------------
    if (t == KALIDOUS_TOKEN_CONST) { advance(p); return parse_var_decl(p, KALIDOUS_BINDING_CONST); }
    if (t == KALIDOUS_TOKEN_LET)   { advance(p); return parse_var_decl(p, KALIDOUS_BINDING_LET);   }
    if (t == KALIDOUS_TOKEN_VAR)   { advance(p); return parse_var_decl(p, KALIDOUS_BINDING_VAR);   }

    // -- Funções — todos os kinds --------------------------------------------
    //
    //   fn name(...)              → NORMAL
    //   async fn name(...)        → ASYNC      (pode usar yield)
    //   noreturn fn name(...)     → NORETURN   (usa goto/marker; sem return)
    //   flowing fn name(...)      → FLOWING    (pode usar goto; tem return)

    if (t == KALIDOUS_TOKEN_FN) {
        advance(p); // consome 'fn'
        return parse_func_body(p, KALIDOUS_FN_NORMAL, loc, is_public);
    }

    if (t == KALIDOUS_TOKEN_ASYNC) {
        advance(p); // consome 'async'
        expect(p, KALIDOUS_TOKEN_FN, "expected 'fn' after 'async'");
        return parse_func_body(p, KALIDOUS_FN_ASYNC, loc, is_public);
    }

    // 'noreturn' e 'flowing' precisarão de tokens próprios ou check_kw
    // Por agora detectados por lexeme enquanto não têm token dedicado
    if (check_kw(p, "noreturn")) {
        advance(p); // consome 'noreturn'
        expect(p, KALIDOUS_TOKEN_FN, "expected 'fn' after 'noreturn'");
        return parse_func_body(p, KALIDOUS_FN_NORETURN, loc, is_public);
    }

    if (check_kw(p, "flowing")) {
        advance(p); // consome 'flowing'
        expect(p, KALIDOUS_TOKEN_FN, "expected 'fn' after 'flowing'");
        return parse_func_body(p, KALIDOUS_FN_FLOWING, loc, is_public);
    }

    // Fallback — expression statement no topo (erro provável)
    fprintf(stderr, "[parse error] line %zu: unexpected token at top level (type %d)\n",
            peek(p)->loc.line, static_cast<int>(t));
    p->had_error = true;
    KalidousNode* expr = parse_expression(p);
    match(p, KALIDOUS_TOKEN_SEMICOLON);
    return expr;
}

// ============================================================================
// Entry point
// ============================================================================

KalidousNode* kalidous_parse(KalidousArena* arena, KalidousTokenStream tokens) {
    Parser p = { arena, tokens.data, tokens.len, 0, false };

    NodeBuilder decls_b;
    while (!is_at_end(&p))
        decls_b.push(arena, parse_declaration(&p));

    // TODO: error recovery — se had_error, tentar sincronizar e continuar

    size_t         count = 0;
    KalidousNode** decls = decls_b.flatten(arena, &count);

    const KalidousSourceLoc root_loc = { 0, 1 };
    return kalidous_ast_make_program(arena, decls, count);
}