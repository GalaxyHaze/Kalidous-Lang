// impl/parser/kalidous_parser.cpp — Recursive descent parser for Kalidous
#include "parser.h"
#include "../memory/utils.h"

// ============================================================================
// Parser init
// ============================================================================

void parser_init(Parser* p, KalidousArena* arena, KalidousTokenStream tokens) {
    p->arena              = arena;
    p->tokens             = tokens.data;
    p->count              = tokens.len;
    p->pos                = 0;
    p->had_error          = false;
    p->fn_kind            = KALIDOUS_FN_NORMAL;
    p->inside_fn          = false;
    p->current_visibility = KALIDOUS_VIS_PRIVATE;
}

// ============================================================================
// Token navigation
// ============================================================================

const KalidousToken* parser_peek(const Parser* p) {
    if (p->pos < p->count) return &p->tokens[p->pos];
    static constexpr KalidousToken eof = { {nullptr, 0}, {0, 0}, KALIDOUS_TOKEN_END, 0 };
    return &eof;
}

const KalidousToken* parser_peek_ahead(const Parser* p, size_t offset) {
    const size_t idx = p->pos + offset;
    if (idx < p->count) return &p->tokens[idx];
    static constexpr KalidousToken eof = { {nullptr, 0}, {0, 0}, KALIDOUS_TOKEN_END, 0 };
    return &eof;
}

const KalidousToken* parser_advance(Parser* p) {
    const KalidousToken* t = parser_peek(p);
    if (p->pos < p->count) p->pos++;
    return t;
}

bool parser_check(const Parser* p, KalidousTokenType type) {
    return parser_peek(p)->type == type;
}

bool parser_match(Parser* p, KalidousTokenType type) {
    if (parser_check(p, type)) { parser_advance(p); return true; }
    return false;
}

const KalidousToken* parser_expect(Parser* p, KalidousTokenType type, const char* msg) {
    if (parser_check(p, type)) return parser_advance(p);
    const KalidousToken* t = parser_peek(p);
    fprintf(stderr, "[parse error] line %zu, col %zu: %s (got '%.*s' — %s)\n",
        t->loc.line, t->loc.index,
        msg,
        (int)t->lexeme.len, t->lexeme.data,
        kalidous_token_type_name(t->type));
    p->had_error = true;
    // TODO: error recovery — skip to synchronization point
    return t;
}

bool parser_is_at_end(const Parser* p) {
    return parser_peek(p)->type == KALIDOUS_TOKEN_END;
}

static bool check_kw(const Parser* p, const char* kw) {
    const KalidousToken* t = parser_peek(p);
    if (t->type != KALIDOUS_TOKEN_IDENTIFIER) return false;
    const size_t len = strlen(kw);
    return t->lexeme.len == len && memcmp(t->lexeme.data, kw, len) == 0;
}

// ============================================================================
// Literal value parsing
// ============================================================================

static bool lexeme_to_cstr(char* buf, size_t buf_size,
                            const char* data, size_t len) {
    if (len >= buf_size) return false;
    memcpy(buf, data, len);
    buf[len] = '\0';
    return true;
}

static KalidousLiteral parse_lit_decimal(const char* data, size_t len) {
    char buf[64];
    if (!lexeme_to_cstr(buf, sizeof(buf), data, len))
        return { KALIDOUS_LIT_INT, { .i64 = 0 } };
    for (size_t i = 0; i < len; ++i) {
        if (buf[i] == '.') {
            KalidousLiteral lit;
            lit.kind      = KALIDOUS_LIT_FLOAT;
            lit.value.f64 = strtod(buf, nullptr);
            return lit;
        }
    }
    KalidousLiteral lit;
    lit.kind      = KALIDOUS_LIT_INT;
    lit.value.i64 = (int64_t)strtoll(buf, nullptr, 10);
    return lit;
}

static KalidousLiteral parse_lit_hex(const char* data, size_t len) {
    char buf[64];
    lexeme_to_cstr(buf, sizeof(buf), data, len);
    KalidousLiteral lit;
    lit.kind      = KALIDOUS_LIT_UINT;
    lit.value.u64 = (uint64_t)strtoull(buf, nullptr, 16);
    return lit;
}

static KalidousLiteral parse_lit_binary(const char* data, size_t len) {
    const char* digits = (len > 2) ? data + 2 : data;
    size_t      dlen   = (len > 2) ? len - 2 : len;
    uint64_t    val    = 0;
    for (size_t i = 0; i < dlen; ++i) {
        if (digits[i] == '0' || digits[i] == '1')
            val = (val << 1) | (uint64_t)(digits[i] - '0');
    }
    return { KALIDOUS_LIT_UINT, { .u64 = val } };
}

static KalidousLiteral parse_lit_octal(const char* data, size_t len) {
    char buf[64];
    const char* src  = (len > 2) ? data + 2 : data;
    size_t      slen = (len > 2) ? len - 2 : len;
    lexeme_to_cstr(buf, sizeof(buf), src, slen);
    KalidousLiteral lit;
    lit.kind      = KALIDOUS_LIT_UINT;
    lit.value.u64 = (uint64_t)strtoull(buf, nullptr, 8);
    return lit;
}

static KalidousLiteral make_lit_string(const char* data, size_t len) {
    if (len >= 2 && data[0] == '"' && data[len - 1] == '"') {
        data += 1;
        len  -= 2;
    }
    KalidousLiteral lit;
    lit.kind             = KALIDOUS_LIT_STRING;
    lit.value.string.ptr = data;
    lit.value.string.len = len;
    return lit;
}

// ============================================================================
// Types
// ============================================================================

KalidousNode* parser_parse_type(Parser* p) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;

    if (parser_match(p, KALIDOUS_TOKEN_UNIQUE) ||
        parser_match(p, KALIDOUS_TOKEN_SHARED) ||
        parser_match(p, KALIDOUS_TOKEN_VIEW)   ||
        parser_match(p, KALIDOUS_TOKEN_LEND)) {
        // TODO: wrap result in KALIDOUS_NODE_TYPE_UNIQUE/SHARED/VIEW/LEND
    }

    if (parser_match(p, KALIDOUS_TOKEN_MULTIPLY)) {
        KalidousNode* inner = parser_parse_type(p);
        // TODO: KALIDOUS_NODE_TYPE_POINTER
        return inner;
    }

    if (parser_match(p, KALIDOUS_TOKEN_LBRACKET)) {
        KalidousNode* size_expr = nullptr;
        if (!parser_check(p, KALIDOUS_TOKEN_RBRACKET))
            size_expr = parser_parse_expression(p);
        parser_expect(p, KALIDOUS_TOKEN_RBRACKET, "expected ']' in array type");
        KalidousNode* inner = parser_parse_type(p);
        (void)size_expr;
        // TODO: KALIDOUS_NODE_TYPE_ARRAY
        return inner;
    }

    if (parser_check(p, KALIDOUS_TOKEN_TYPE) || parser_check(p, KALIDOUS_TOKEN_IDENTIFIER)) {
        const KalidousToken* t = parser_advance(p);
        KalidousNode* base = kalidous_ast_make_identifier(p->arena, loc,
                                                          t->lexeme.data, t->lexeme.len);
        if (parser_match(p, KALIDOUS_TOKEN_QUESTION)) {
            // TODO: KALIDOUS_NODE_TYPE_OPTIONAL
        } else if (parser_match(p, KALIDOUS_TOKEN_BANG)) {
            // TODO: KALIDOUS_NODE_TYPE_RESULT
        }
        return base;
    }

    fprintf(stderr, "[parse error] line %zu: expected type\n", loc.line);
    p->had_error = true;
    return kalidous_ast_make_error(p->arena, loc, "expected type");
}

// ============================================================================
// Expressions — Pratt parser
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
    const KalidousSourceLoc loc = parser_peek(p)->loc;
    const KalidousToken* t = parser_advance(p);

    switch (t->type) {

        case KALIDOUS_TOKEN_NUMBER:
            return kalidous_ast_make_literal(p->arena, loc,
                       parse_lit_decimal(t->lexeme.data, t->lexeme.len));
        case KALIDOUS_TOKEN_FLOAT:
            return kalidous_ast_make_literal(p->arena, loc,
                       parse_lit_decimal(t->lexeme.data, t->lexeme.len));
        case KALIDOUS_TOKEN_STRING:
            return kalidous_ast_make_literal(p->arena, loc,
                       make_lit_string(t->lexeme.data, t->lexeme.len));
        case KALIDOUS_TOKEN_HEXADECIMAL:
            return kalidous_ast_make_literal(p->arena, loc,
                       parse_lit_hex(t->lexeme.data, t->lexeme.len));
        case KALIDOUS_TOKEN_BINARY:
            return kalidous_ast_make_literal(p->arena, loc,
                       parse_lit_binary(t->lexeme.data, t->lexeme.len));
        case KALIDOUS_TOKEN_OCTAL:
            return kalidous_ast_make_literal(p->arena, loc,
                       parse_lit_octal(t->lexeme.data, t->lexeme.len));

        case KALIDOUS_TOKEN_IDENTIFIER: {
            KalidousNode* ident = kalidous_ast_make_identifier(p->arena, loc,
                                      t->lexeme.data, t->lexeme.len);
            if (!parser_check(p, KALIDOUS_TOKEN_LPAREN)) return ident;
            parser_advance(p);
            ArenaList<KalidousNode*> args_b;
            args_b.init(p->arena, 8);
            while (!parser_check(p, KALIDOUS_TOKEN_RPAREN) && !parser_is_at_end(p)) {
                args_b.push(p->arena, parser_parse_expression(p));
                if (!parser_match(p, KALIDOUS_TOKEN_COMMA)) break;
            }
            parser_expect(p, KALIDOUS_TOKEN_RPAREN, "expected ')' after arguments");
            size_t         arg_count = 0;
            KalidousNode** args      = args_b.flatten(p->arena, &arg_count);
            return kalidous_ast_make_call(p->arena, loc, ident, args, arg_count);
        }

        case KALIDOUS_TOKEN_RECURSE: {
            if (!p->inside_fn) {
                fprintf(stderr, "[parse error] line %zu: 'recurse' outside function\n",
                        loc.line);
                p->had_error = true;
            }
            KalidousNode* self_ref = kalidous_ast_make_identifier(p->arena, loc,
                                         t->lexeme.data, t->lexeme.len);
            parser_expect(p, KALIDOUS_TOKEN_LPAREN, "expected '(' after recurse");
            ArenaList<KalidousNode*> args_b;
            args_b.init(p->arena, 8);
            while (!parser_check(p, KALIDOUS_TOKEN_RPAREN) && !parser_is_at_end(p)) {
                args_b.push(p->arena, parser_parse_expression(p));
                if (!parser_match(p, KALIDOUS_TOKEN_COMMA)) break;
            }
            parser_expect(p, KALIDOUS_TOKEN_RPAREN, "expected ')' after recurse arguments");
            size_t         arg_count = 0;
            KalidousNode** args      = args_b.flatten(p->arena, &arg_count);
            return kalidous_ast_make_recurse(p->arena, loc, self_ref, args, arg_count);
        }

        case KALIDOUS_TOKEN_MINUS:
            return kalidous_ast_make_unary_op(p->arena, loc, KALIDOUS_TOKEN_MINUS,
                                              parse_expr_bp(p, 13), false);
        case KALIDOUS_TOKEN_BANG:
            return kalidous_ast_make_unary_op(p->arena, loc, KALIDOUS_TOKEN_BANG,
                                              parse_expr_bp(p, 13), false);

        case KALIDOUS_TOKEN_LPAREN: {
            KalidousNode* expr = parser_parse_expression(p);
            // TODO: distinguish grouping from tuple (a, b, c)
            parser_expect(p, KALIDOUS_TOKEN_RPAREN, "expected ')'");
            return expr;
        }

        // Positional struct/array literal: { expr, expr, ... }
        // Comma-separated expressions inside braces — distinct from a block (uses ';')
        case KALIDOUS_TOKEN_LBRACE: {
            ArenaList<KalidousNode*> items_b;
            items_b.init(p->arena, 4);
            while (!parser_check(p, KALIDOUS_TOKEN_RBRACE) && !parser_is_at_end(p)) {
                items_b.push(p->arena, parser_parse_expression(p));
                if (!parser_match(p, KALIDOUS_TOKEN_COMMA)) break;
            }
            parser_expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}' after struct literal");
            size_t         count = 0;
            KalidousNode** items = items_b.flatten(p->arena, &count);
            KalidousNode*  n     = kalidous_ast_make_block(p->arena, loc, items, count);
            if (n) n->type = KALIDOUS_NODE_STRUCT_LIT;
            return n;
        }

        case KALIDOUS_TOKEN_SPAWN: {
            KalidousNode* expr = parser_parse_expression(p);
            return kalidous_ast_make_spawn(p->arena, loc, expr, false);
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

    while (true) {
        const KalidousTokenType op = parser_peek(p)->type;
        const BindingPower      bp = infix_bp(op);
        if (bp.left < min_bp) break;

        const KalidousSourceLoc loc = parser_peek(p)->loc;
        parser_advance(p);

        if (op == KALIDOUS_TOKEN_DOT) {
            const KalidousToken* member = parser_expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                                        "expected member name after '.'");
            KalidousNode* rhs = kalidous_ast_make_identifier(p->arena, member->loc,
                                    member->lexeme.data, member->lexeme.len);
            left = kalidous_ast_make_member(p->arena, loc, left, rhs);
            continue;
        }
        if (op == KALIDOUS_TOKEN_ARROW) {
            KalidousNode* rhs = parse_expr_bp(p, bp.right);
            left = kalidous_ast_make_arrow_call(p->arena, loc, left, rhs);
            continue;
        }

        KalidousNode* right = parse_expr_bp(p, bp.right);
        left = kalidous_ast_make_binary_op(p->arena, loc, op, left, right);
    }

    while (true) {
        const KalidousSourceLoc loc = parser_peek(p)->loc;
        if (parser_match(p, KALIDOUS_TOKEN_QUESTION)) {
            left = kalidous_ast_make_unary_op(p->arena, loc,
                                              KALIDOUS_TOKEN_QUESTION, left, true);
        } else if (parser_match(p, KALIDOUS_TOKEN_BANG)) {
            left = kalidous_ast_make_unary_op(p->arena, loc,
                                              KALIDOUS_TOKEN_BANG, left, true);
        } else {
            // TODO: 'as' cast
            break;
        }
    }

    return left;
}

KalidousNode* parser_parse_expression(Parser* p) {
    return parse_expr_bp(p, 0);
}

// ============================================================================
// Statements
// ============================================================================

static KalidousNode* parse_var_decl(Parser* p, KalidousBindingKind binding) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;

    KalidousOwnership own = KALIDOUS_OWN_DEFAULT;
    if      (parser_match(p, KALIDOUS_TOKEN_UNIQUE)) own = KALIDOUS_OWN_UNIQUE;
    else if (parser_match(p, KALIDOUS_TOKEN_SHARED)) own = KALIDOUS_OWN_SHARED;
    else if (parser_match(p, KALIDOUS_TOKEN_VIEW))   own = KALIDOUS_OWN_VIEW;
    else if (parser_match(p, KALIDOUS_TOKEN_LEND))   own = KALIDOUS_OWN_LEND;

    const KalidousToken* name = parser_expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                              "expected variable name");

    KalidousNode* type_node = nullptr;
    if (parser_match(p, KALIDOUS_TOKEN_COLON))
        type_node = parser_parse_type(p);

    KalidousNode* initializer = nullptr;
    if (parser_match(p, KALIDOUS_TOKEN_ASSIGNMENT) ||
        parser_match(p, KALIDOUS_TOKEN_DECLARATION))
        initializer = parser_parse_expression(p);

    parser_expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after variable declaration");

    const KalidousVarPayload decl = {
        name->lexeme.data, name->lexeme.len,
        binding, own, KALIDOUS_VIS_PRIVATE,
        type_node, initializer
    };
    return kalidous_ast_make_var_decl(p->arena, loc, decl);
}

static KalidousNode* parse_if(Parser* p) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p);
    KalidousNode* cond    = parser_parse_expression(p);
    KalidousNode* then_br = parser_parse_block(p);
    KalidousNode* else_br = nullptr;
    if (parser_match(p, KALIDOUS_TOKEN_ELSE)) {
        else_br = parser_check(p, KALIDOUS_TOKEN_IF)
            ? parse_if(p)
            : parser_parse_block(p);
    }
    return kalidous_ast_make_if(p->arena, loc, cond, then_br, else_br);
}

static KalidousNode* parse_for(Parser* p) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p);
    KalidousForPayload data = {};

    if (parser_check(p, KALIDOUS_TOKEN_IDENTIFIER) &&
        parser_peek_ahead(p, 1)->type == KALIDOUS_TOKEN_IN) {
        data.is_for_in    = true;
        data.iterator_var = parser_parse_expression(p);
        parser_advance(p);
        data.iterable     = parser_parse_expression(p);
        data.body         = parser_parse_block(p);
        return kalidous_ast_make_for(p->arena, loc, data);
    }
    // TODO: for init; cond; step { }
    data.is_for_in = false;
    data.condition = parser_parse_expression(p);
    data.body      = parser_parse_block(p);
    return kalidous_ast_make_for(p->arena, loc, data);
}

static KalidousNode* parse_switch(Parser* p) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p);
    KalidousNode* subject = parser_parse_expression(p);
    parser_expect(p, KALIDOUS_TOKEN_LBRACE, "expected '{' after switch expression");
    ArenaList<KalidousNode*> arms_b;
    arms_b.init(p->arena, 8);
    KalidousNode* default_arm = nullptr;
    // TODO: parse case pattern [if guard]: stmt*
    while (!parser_check(p, KALIDOUS_TOKEN_RBRACE) && !parser_is_at_end(p))
        parser_advance(p);
    parser_expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}'");
    size_t         arm_count = 0;
    KalidousNode** arms      = arms_b.flatten(p->arena, &arm_count);
    const KalidousSwitchPayload data = { subject, arms, arm_count, default_arm };
    return kalidous_ast_make_switch(p->arena, loc, data);
}

static KalidousNode* parse_try_catch(Parser* p) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p);
    KalidousNode* try_block = parser_parse_block(p);
    KalidousTryCatchPayload data = { try_block, nullptr, 0, nullptr };
    if (parser_match(p, KALIDOUS_TOKEN_CATCH)) {
        if (parser_check(p, KALIDOUS_TOKEN_IDENTIFIER)) {
            const KalidousToken* evar = parser_advance(p);
            data.catch_var     = evar->lexeme.data;
            data.catch_var_len = evar->lexeme.len;
        }
        // TODO: catch e: ErrorType, multiple catch, finally
        data.catch_block = parser_parse_block(p);
    }
    return kalidous_ast_make_try_catch(p->arena, loc, data);
}

static KalidousNode* parse_return(Parser* p) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p);
    if (p->inside_fn && p->fn_kind == KALIDOUS_FN_NORETURN) {
        fprintf(stderr, "[parse error] line %zu: 'return' not allowed in noreturn fn\n",
                loc.line);
        p->had_error = true;
    }
    KalidousNode* value = nullptr;
    if (!parser_check(p, KALIDOUS_TOKEN_SEMICOLON) && !parser_is_at_end(p))
        value = parser_parse_expression(p);
    parser_expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after return");
    return kalidous_ast_make_return(p->arena, loc, value);
}

static KalidousNode* parse_yield(Parser* p) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p);
    if (!p->inside_fn || p->fn_kind != KALIDOUS_FN_ASYNC) {
        fprintf(stderr, "[parse error] line %zu: 'yield' only allowed in async fn\n",
                loc.line);
        p->had_error = true;
    }
    KalidousNode* value = nullptr;
    if (!parser_check(p, KALIDOUS_TOKEN_SEMICOLON) && !parser_is_at_end(p))
        value = parser_parse_expression(p);
    parser_expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after yield");
    return kalidous_ast_make_yield(p->arena, loc, value);
}

KalidousNode* parser_parse_block(Parser* p) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;
    parser_expect(p, KALIDOUS_TOKEN_LBRACE, "expected '{'");
    ArenaList<KalidousNode*> stmts_b;
    stmts_b.init(p->arena, 16);
    while (!parser_check(p, KALIDOUS_TOKEN_RBRACE) && !parser_is_at_end(p))
        stmts_b.push(p->arena, parser_parse_statement(p));
    parser_expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}'");
    size_t         count = 0;
    KalidousNode** stmts = stmts_b.flatten(p->arena, &count);
    return kalidous_ast_make_block(p->arena, loc, stmts, count);
}

KalidousNode* parser_parse_statement(Parser* p) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;

    switch (parser_peek(p)->type) {
        case KALIDOUS_TOKEN_LET:   parser_advance(p); return parse_var_decl(p, KALIDOUS_BINDING_LET);
        case KALIDOUS_TOKEN_VAR:   parser_advance(p); return parse_var_decl(p, KALIDOUS_BINDING_VAR);
        case KALIDOUS_TOKEN_AUTO:  parser_advance(p); return parse_var_decl(p, KALIDOUS_BINDING_AUTO);
        case KALIDOUS_TOKEN_CONST: parser_advance(p); return parse_var_decl(p, KALIDOUS_BINDING_CONST);
        case KALIDOUS_TOKEN_IF:     return parse_if(p);
        case KALIDOUS_TOKEN_FOR:    return parse_for(p);
        case KALIDOUS_TOKEN_SWITCH: return parse_switch(p);
        case KALIDOUS_TOKEN_TRY:    return parse_try_catch(p);
        case KALIDOUS_TOKEN_RETURN: return parse_return(p);
        case KALIDOUS_TOKEN_YIELD:  return parse_yield(p);

        // Anonymous scoped block: { ... }
        // Distinct from struct literal — { at statement position is always a block
        case KALIDOUS_TOKEN_LBRACE:
            return parser_parse_block(p);

        case KALIDOUS_TOKEN_AWAIT: {
            parser_advance(p);
            KalidousNode* expr = parser_parse_expression(p);
            parser_expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after await");
            return kalidous_ast_make_await(p->arena, loc, expr);
        }
        case KALIDOUS_TOKEN_BREAK: {
            parser_advance(p);
            parser_expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after break");
            return kalidous_ast_make_break(p->arena, loc, nullptr, 0);
            // TODO: labelled break
        }
        case KALIDOUS_TOKEN_CONTINUE: {
            parser_advance(p);
            parser_expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after continue");
            return kalidous_ast_make_continue(p->arena, loc, nullptr, 0);
            // TODO: labelled continue
        }
        case KALIDOUS_TOKEN_GOTO: {
            parser_advance(p);
            if (p->inside_fn && p->fn_kind == KALIDOUS_FN_NORMAL) {
                fprintf(stderr,
                    "[parse error] line %zu: 'goto' not allowed in normal fn\n", loc.line);
                p->had_error = true;
            }
            if (p->inside_fn && p->fn_kind == KALIDOUS_FN_ASYNC) {
                fprintf(stderr,
                    "[parse error] line %zu: 'goto' not allowed in async fn\n", loc.line);
                p->had_error = true;
            }
            const KalidousToken* label = parser_expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                                       "expected label after goto");
            parser_expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after goto");
            return kalidous_ast_make_goto(p->arena, loc,
                                          label->lexeme.data, label->lexeme.len);
        }
        case KALIDOUS_TOKEN_MARKER: {
            parser_advance(p);
            if (p->inside_fn && p->fn_kind == KALIDOUS_FN_NORMAL) {
                fprintf(stderr,
                    "[parse error] line %zu: 'marker' not allowed in normal fn\n", loc.line);
                p->had_error = true;
            }
            const KalidousToken* label = parser_expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                                       "expected label after marker");
            parser_expect(p, KALIDOUS_TOKEN_COLON, "expected ':' after marker label");
            return kalidous_ast_make_marker(p->arena, loc,
                                            label->lexeme.data, label->lexeme.len);
        }
        case KALIDOUS_TOKEN_SPAWN: {
            parser_advance(p);
            if (parser_check(p, KALIDOUS_TOKEN_LBRACE)) {
                KalidousNode* body = parser_parse_block(p);
                return kalidous_ast_make_spawn(p->arena, loc, body, true);
            }
            KalidousNode* expr = parser_parse_expression(p);
            parser_expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after spawn expression");
            return kalidous_ast_make_spawn(p->arena, loc, expr, false);
        }

        default: {
            KalidousNode* expr = parser_parse_expression(p);
            const KalidousTokenType op = parser_peek(p)->type;
            if (op == KALIDOUS_TOKEN_ASSIGNMENT   ||
                op == KALIDOUS_TOKEN_DECLARATION  ||
                op == KALIDOUS_TOKEN_PLUS_EQUAL   ||
                op == KALIDOUS_TOKEN_MINUS_EQUAL  ||
                op == KALIDOUS_TOKEN_MULTIPLY_EQUAL ||
                op == KALIDOUS_TOKEN_DIVIDE_EQUAL) {
                const KalidousSourceLoc assign_loc = parser_peek(p)->loc;
                parser_advance(p);
                KalidousNode* value = parser_parse_expression(p);
                expr = kalidous_ast_make_binary_op(p->arena, assign_loc, op, expr, value);
            }
            parser_expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after expression statement");
            return expr;
        }
    }
}

// ============================================================================
// Top-level declarations
// ============================================================================

static KalidousVisibility resolve_visibility(const KalidousToken* t) {
    const char*  d = t->lexeme.data;
    const size_t l = t->lexeme.len;
    if (l == 6 && strncmp(d, "public",    6) == 0) return KALIDOUS_VIS_PUBLIC;
    if (l == 9 && strncmp(d, "protected", 9) == 0) return KALIDOUS_VIS_PROTECTED;
    return KALIDOUS_VIS_PRIVATE;
}

static bool is_visibility_group(const Parser* p) {
    return parser_check(p, KALIDOUS_TOKEN_MODIFIER) &&
           parser_peek_ahead(p, 1)->type == KALIDOUS_TOKEN_COLON;
}

static bool is_visibility_inline(const Parser* p) {
    return parser_check(p, KALIDOUS_TOKEN_MODIFIER) &&
           parser_peek_ahead(p, 1)->type != KALIDOUS_TOKEN_COLON;
}

static KalidousNode* parse_field(Parser* p, KalidousVisibility vis) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;

    KalidousOwnership own = KALIDOUS_OWN_DEFAULT;
    if      (parser_match(p, KALIDOUS_TOKEN_UNIQUE)) own = KALIDOUS_OWN_UNIQUE;
    else if (parser_match(p, KALIDOUS_TOKEN_SHARED)) own = KALIDOUS_OWN_SHARED;
    else if (parser_match(p, KALIDOUS_TOKEN_VIEW))   own = KALIDOUS_OWN_VIEW;
    else if (parser_match(p, KALIDOUS_TOKEN_LEND))   own = KALIDOUS_OWN_LEND;

    const KalidousToken* name = parser_expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                              "expected field name");
    // Type annotation is optional: let name = default  vs  name: Type
    KalidousNode* type_node = nullptr;
    if (parser_match(p, KALIDOUS_TOKEN_COLON))
        type_node = parser_parse_type(p);

    KalidousNode* default_value = nullptr;
    if (parser_match(p, KALIDOUS_TOKEN_ASSIGNMENT))
        default_value = parser_parse_expression(p);

    parser_expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' after field declaration");

    const KalidousFieldPayload field = {
        name->lexeme.data, name->lexeme.len,
        own, vis, type_node, default_value
    };
    return kalidous_ast_make_field(p->arena, loc, field);
}

static KalidousNode* parse_enum_variant(Parser* p) {
    const KalidousSourceLoc loc  = parser_peek(p)->loc;
    const KalidousToken*    name = parser_expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                                 "expected variant name");
    KalidousNode* value = nullptr;
    if (parser_match(p, KALIDOUS_TOKEN_ASSIGNMENT))
        value = parser_parse_expression(p);
    // TODO: payload variants for family: Red { r: u8, g: u8, b: u8 }
    return kalidous_ast_make_enum_variant(p->arena, loc,
                                          name->lexeme.data, name->lexeme.len, value);
}

static KalidousNode* parse_param(Parser* p) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;
    KalidousOwnership own        = KALIDOUS_OWN_DEFAULT;
    bool              is_mutable = false;
    if      (parser_match(p, KALIDOUS_TOKEN_UNIQUE)) own = KALIDOUS_OWN_UNIQUE;
    else if (parser_match(p, KALIDOUS_TOKEN_SHARED)) own = KALIDOUS_OWN_SHARED;
    else if (parser_match(p, KALIDOUS_TOKEN_VIEW))   own = KALIDOUS_OWN_VIEW;
    else if (parser_match(p, KALIDOUS_TOKEN_LEND))   { own = KALIDOUS_OWN_LEND; is_mutable = true; }
    const KalidousToken* name = parser_expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                              "expected parameter name");
    parser_expect(p, KALIDOUS_TOKEN_COLON, "expected ':' after parameter name");
    KalidousNode* type_node = parser_parse_type(p);
    KalidousNode* default_value = nullptr;
    if (parser_match(p, KALIDOUS_TOKEN_ASSIGNMENT))
        default_value = parser_parse_expression(p);
    const KalidousParamPayload param = {
        name->lexeme.data, name->lexeme.len,
        own, type_node, default_value, is_mutable
    };
    return kalidous_ast_make_param(p->arena, loc, param);
}

// Forward declaration — needed by parse_struct_decl before the full definition
static KalidousNode* parse_func_body(Parser* p, KalidousFnKind kind,
                                     KalidousSourceLoc loc, KalidousVisibility visibility);

static KalidousNode* parse_struct_decl(Parser* p, KalidousVisibility struct_vis) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p);

    const KalidousToken* name = parser_expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                              "expected struct name");
    parser_expect(p, KALIDOUS_TOKEN_LBRACE, "expected '{'");

    ArenaList<KalidousNode*> fields_b, methods_b;
    fields_b.init(p->arena, 8);
    methods_b.init(p->arena, 4);

    KalidousVisibility current_vis = KALIDOUS_VIS_PRIVATE;

    while (!parser_check(p, KALIDOUS_TOKEN_RBRACE) && !parser_is_at_end(p)) {

        // Group: public: / private: / protected:
        if (is_visibility_group(p)) {
            current_vis = resolve_visibility(parser_peek(p));
            parser_advance(p);
            parser_advance(p);
            continue;
        }

        // Inline: public fn foo() or public name: Type
        KalidousVisibility item_vis = current_vis;
        if (is_visibility_inline(p)) {
            item_vis = resolve_visibility(parser_peek(p));
            parser_advance(p);
        }

        // Inline method
        if (parser_check(p, KALIDOUS_TOKEN_FN)) {
            const KalidousSourceLoc fn_loc = parser_peek(p)->loc;
            parser_advance(p);
            methods_b.push(p->arena, parse_func_body(p, KALIDOUS_FN_NORMAL,
                                                     fn_loc, item_vis));
            continue;
        }

        // Field with binding keyword: let name [: Type] [= default];
        if (parser_check(p, KALIDOUS_TOKEN_LET)  ||
            parser_check(p, KALIDOUS_TOKEN_VAR)  ||
            parser_check(p, KALIDOUS_TOKEN_CONST)) {
            parser_advance(p);
            fields_b.push(p->arena, parse_field(p, item_vis));
            continue;
        }

        // Field without keyword: name: Type [= default];
        if (parser_check(p, KALIDOUS_TOKEN_IDENTIFIER)) {
            fields_b.push(p->arena, parse_field(p, item_vis));
            continue;
        }

        // Ownership modifier before field name
        if (parser_check(p, KALIDOUS_TOKEN_UNIQUE) ||
            parser_check(p, KALIDOUS_TOKEN_SHARED) ||
            parser_check(p, KALIDOUS_TOKEN_VIEW)   ||
            parser_check(p, KALIDOUS_TOKEN_LEND)) {
            fields_b.push(p->arena, parse_field(p, item_vis));
            continue;
        }

        fprintf(stderr, "[parse error] line %zu: unexpected token in struct body\n",
                parser_peek(p)->loc.line);
        p->had_error = true;
        parser_advance(p);
    }
    parser_expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}'");

    size_t         field_count  = 0;
    KalidousNode** fields       = fields_b.flatten(p->arena, &field_count);
    size_t         method_count = 0;
    KalidousNode** methods      = methods_b.flatten(p->arena, &method_count);

    const KalidousStructPayload decl = {
        name->lexeme.data, name->lexeme.len,
        fields, field_count, methods, method_count,
        struct_vis
    };
    return kalidous_ast_make_struct(p->arena, loc, decl);
}

static KalidousNode* parse_enum_decl(Parser* p, KalidousVisibility enum_vis) {
    const KalidousSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p);
    const KalidousToken* name = parser_expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                              "expected enum name");
    parser_expect(p, KALIDOUS_TOKEN_LBRACE, "expected '{'");

    ArenaList<KalidousNode*> variants_b;
    variants_b.init(p->arena, 8);
    while (!parser_check(p, KALIDOUS_TOKEN_RBRACE) && !parser_is_at_end(p)) {
        variants_b.push(p->arena, parse_enum_variant(p));
        if (!parser_match(p, KALIDOUS_TOKEN_COMMA)) break;
    }
    parser_expect(p, KALIDOUS_TOKEN_RBRACE, "expected '}'");

    size_t         variant_count = 0;
    KalidousNode** variants      = variants_b.flatten(p->arena, &variant_count);

    const KalidousEnumPayload decl = {
        name->lexeme.data, name->lexeme.len,
        variants, variant_count, enum_vis
    };
    return kalidous_ast_make_enum(p->arena, loc, decl);
}

// Full implementation of the forward-declared parse_func_body
static KalidousNode* parse_func_body(Parser* p, KalidousFnKind kind,
                                     KalidousSourceLoc loc, KalidousVisibility visibility) {
    const KalidousToken* name = parser_expect(p, KALIDOUS_TOKEN_IDENTIFIER,
                                              "expected function name");
    parser_expect(p, KALIDOUS_TOKEN_LPAREN, "expected '(' after function name");

    ArenaList<KalidousNode*> params_b;
    params_b.init(p->arena, 8);
    while (!parser_check(p, KALIDOUS_TOKEN_RPAREN) && !parser_is_at_end(p)) {
        params_b.push(p->arena, parse_param(p));
        if (!parser_match(p, KALIDOUS_TOKEN_COMMA)) break;
    }
    parser_expect(p, KALIDOUS_TOKEN_RPAREN, "expected ')' after parameters");

    KalidousNode* return_type = nullptr;
    if (kind != KALIDOUS_FN_NORETURN && parser_match(p, KALIDOUS_TOKEN_COLON))
        return_type = parser_parse_type(p);

    const KalidousFnKind outer_kind   = p->fn_kind;
    const bool           outer_inside = p->inside_fn;
    p->fn_kind   = kind;
    p->inside_fn = true;

    KalidousNode* body = nullptr;
    if (parser_check(p, KALIDOUS_TOKEN_LBRACE))
        body = parser_parse_block(p);
    else
        parser_expect(p, KALIDOUS_TOKEN_SEMICOLON, "expected ';' for forward declaration");

    p->fn_kind   = outer_kind;
    p->inside_fn = outer_inside;

    size_t         param_count = 0;
    KalidousNode** params      = params_b.flatten(p->arena, &param_count);

    const KalidousFuncPayload decl = {
        name->lexeme.data, name->lexeme.len,
        kind, params, param_count,
        return_type, body,
        visibility, false
    };
    return kalidous_ast_make_func_decl(p->arena, loc, decl);
}

KalidousNode* parser_parse_declaration(Parser* p) {
    KalidousVisibility vis = p->current_visibility;
    if (is_visibility_inline(p)) {
        vis = resolve_visibility(parser_peek(p));
        parser_advance(p);
    }
    if (is_visibility_group(p)) {
        p->current_visibility = resolve_visibility(parser_peek(p));
        parser_advance(p);
        parser_advance(p);
        return nullptr; // group modifier — no node, just updates context
    }

    const KalidousSourceLoc loc = parser_peek(p)->loc;
    const KalidousTokenType t   = parser_peek(p)->type;

    if (t == KALIDOUS_TOKEN_STRUCT) return parse_struct_decl(p, vis);
    if (t == KALIDOUS_TOKEN_ENUM)   return parse_enum_decl(p, vis);

    // TODO: COMPONENT, UNION, FAMILY, ENTITY, TRAIT, IMPLEMENT, MODULE
    // TODO: use / import

    if (t == KALIDOUS_TOKEN_CONST) { parser_advance(p); return parse_var_decl(p, KALIDOUS_BINDING_CONST); }
    if (t == KALIDOUS_TOKEN_LET)   { parser_advance(p); return parse_var_decl(p, KALIDOUS_BINDING_LET);   }
    if (t == KALIDOUS_TOKEN_VAR)   { parser_advance(p); return parse_var_decl(p, KALIDOUS_BINDING_VAR);   }

    if (t == KALIDOUS_TOKEN_FN) {
        parser_advance(p);
        return parse_func_body(p, KALIDOUS_FN_NORMAL, loc, vis);
    }
    if (t == KALIDOUS_TOKEN_ASYNC) {
        parser_advance(p);
        parser_expect(p, KALIDOUS_TOKEN_FN, "expected 'fn' after 'async'");
        return parse_func_body(p, KALIDOUS_FN_ASYNC, loc, vis);
    }
    if (check_kw(p, "noreturn")) {
        parser_advance(p);
        parser_expect(p, KALIDOUS_TOKEN_FN, "expected 'fn' after 'noreturn'");
        return parse_func_body(p, KALIDOUS_FN_NORETURN, loc, vis);
    }
    if (check_kw(p, "flowing")) {
        parser_advance(p);
        parser_expect(p, KALIDOUS_TOKEN_FN, "expected 'fn' after 'flowing'");
        return parse_func_body(p, KALIDOUS_FN_FLOWING, loc, vis);
    }

    fprintf(stderr, "[parse error] line %zu: unexpected token at top level (type %d)\n",
            parser_peek(p)->loc.line, static_cast<int>(t));
    p->had_error = true;
    KalidousNode* expr = parser_parse_expression(p);
    parser_match(p, KALIDOUS_TOKEN_SEMICOLON);
    return expr;
}

// ============================================================================
// Entry point — implements kalidous_parse declared in kalidous.h
// ============================================================================

KalidousNode* kalidous_parse(KalidousArena* arena, KalidousTokenStream tokens) {
    Parser p;
    parser_init(&p, arena, tokens);

    ArenaList<KalidousNode*> decls_b;
    decls_b.init(arena, 16);

    while (!parser_is_at_end(&p)) {
        KalidousNode* decl = parser_parse_declaration(&p);
        if (decl) decls_b.push(arena, decl); // nullptr = group visibility modifier, skip
    }

    // TODO: error recovery — accumulate errors instead of stopping at first

    size_t         count = 0;
    KalidousNode** decls = decls_b.flatten(arena, &count);
    return kalidous_ast_make_program(arena, decls, count);
}