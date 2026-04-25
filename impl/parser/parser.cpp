// impl/parser/parser.cpp — Parser entry point and pipeline orchestration
#include "../memory/arena.hpp"
#include "../lexer/debug.h"
#include "parser.h"
#include <ankerl/unordered_dense.h>
#include <cstdio>
#include <cstring>
#include <string>

using zith::ArenaList;

namespace {

enum class SemaType {
    Unknown,
    Void,
    Int,
    Float,
    String,
    Bool,
};

struct SemaScope {
    ankerl::unordered_dense::map<std::string, SemaType> vars;
};

struct SemaContext {
    Parser *p;
    ankerl::unordered_dense::map<std::string, ZithFuncPayload *> functions;
    std::vector<SemaScope> scopes;
};

static std::string ident_name(const ZithNode *n) {
    if (!n || n->type != ZITH_NODE_IDENTIFIER || !n->data.ident.str) return {};
    return std::string(n->data.ident.str, n->data.ident.len);
}

static const char *type_name(SemaType t) {
    switch (t) {
        case SemaType::Void: return "void";
        case SemaType::Int: return "int";
        case SemaType::Float: return "float";
        case SemaType::String: return "string";
        case SemaType::Bool: return "bool";
        default: return "unknown";
    }
}

static SemaType sema_type_from_node(const ZithNode *n) {
    if (!n) return SemaType::Unknown;
    if (n->type == ZITH_NODE_IDENTIFIER && n->data.ident.str) {
        const std::string name(n->data.ident.str, n->data.ident.len);
        if (name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
            name == "u8" || name == "u16" || name == "u32" || name == "u64" ||
            name == "int") return SemaType::Int;
        if (name == "f32" || name == "f64" || name == "float") return SemaType::Float;
        if (name == "str" || name == "string") return SemaType::String;
        if (name == "bool") return SemaType::Bool;
        if (name == "void") return SemaType::Void;
    }
    return SemaType::Unknown;
}

static void sema_push_scope(SemaContext &ctx) { ctx.scopes.push_back({}); }
static void sema_pop_scope(SemaContext &ctx) { if (!ctx.scopes.empty()) ctx.scopes.pop_back(); }

static void sema_define(SemaContext &ctx, const std::string &name, SemaType t) {
    if (ctx.scopes.empty()) sema_push_scope(ctx);
    ctx.scopes.back().vars[name] = t;
}

static SemaType sema_lookup(const SemaContext &ctx, const std::string &name) {
    for (auto it = ctx.scopes.rbegin(); it != ctx.scopes.rend(); ++it) {
        auto found = it->vars.find(name);
        if (found != it->vars.end()) return found->second;
    }
    return SemaType::Unknown;
}

static SemaType sema_expr(SemaContext &ctx, ZithNode *expr);

static void sema_stmt(SemaContext &ctx, ZithNode *stmt) {
    if (!stmt) return;
    switch (stmt->type) {
        case ZITH_NODE_VAR_DECL: {
            auto *var = static_cast<ZithVarPayload *>(stmt->data.list.ptr);
            const std::string name(var->name, var->name_len);
            const SemaType declared = sema_type_from_node(var->type_node);
            const SemaType init = sema_expr(ctx, var->initializer);
            if (declared != SemaType::Unknown && init != SemaType::Unknown && declared != init) {
                char buf[256];
                snprintf(buf, sizeof(buf), "type mismatch in '%s': expected %s, got %s",
                         name.c_str(), type_name(declared), type_name(init));
                parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR, buf);
            }
            sema_define(ctx, name, declared != SemaType::Unknown ? declared : init);
            break;
        }
        case ZITH_NODE_RETURN:
            sema_expr(ctx, stmt->data.kids.a);
            break;
        case ZITH_NODE_IF:
            sema_expr(ctx, stmt->data.kids.a);
            sema_stmt(ctx, stmt->data.kids.b);
            sema_stmt(ctx, stmt->data.kids.c);
            break;
        case ZITH_NODE_BLOCK: {
            sema_push_scope(ctx);
            auto **stmts = static_cast<ZithNode **>(stmt->data.list.ptr);
            for (size_t i = 0; i < stmt->data.list.len; ++i) sema_stmt(ctx, stmts[i]);
            sema_pop_scope(ctx);
            break;
        }
        default:
            sema_expr(ctx, stmt);
            break;
    }
}

static SemaType sema_expr(SemaContext &ctx, ZithNode *expr) {
    if (!expr) return SemaType::Void;
    switch (expr->type) {
        case ZITH_NODE_LITERAL: {
            auto *lit = static_cast<ZithLiteral *>(expr->data.list.ptr);
            if (!lit) return SemaType::Unknown;
            switch (lit->kind) {
                case ZITH_LIT_INT:
                case ZITH_LIT_UINT: return SemaType::Int;
                case ZITH_LIT_FLOAT: return SemaType::Float;
                case ZITH_LIT_STRING: return SemaType::String;
                case ZITH_LIT_BOOL: return SemaType::Bool;
            }
            return SemaType::Unknown;
        }
        case ZITH_NODE_IDENTIFIER: {
            const std::string name = ident_name(expr);
            const SemaType t = sema_lookup(ctx, name);
            if (t == SemaType::Unknown && !name.empty()) {
                char buf[256];
                snprintf(buf, sizeof(buf), "undefined identifier '%s'", name.c_str());
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
            }
            return t;
        }
        case ZITH_NODE_CALL: {
            auto *call = static_cast<ZithCallPayload *>(expr->data.list.ptr);
            const std::string callee_name = ident_name(call ? call->callee : nullptr);
            if (callee_name == "print" || callee_name == "println") {
                for (size_t i = 0; call && i < call->arg_count; ++i) sema_expr(ctx, call->args[i]);
                return SemaType::Void;
            }
            auto f = ctx.functions.find(callee_name);
            if (f == ctx.functions.end()) {
                char buf[256];
                snprintf(buf, sizeof(buf), "undefined function '%s'", callee_name.c_str());
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
                return SemaType::Unknown;
            }
            for (size_t i = 0; call && i < call->arg_count; ++i) sema_expr(ctx, call->args[i]);
            return sema_type_from_node(f->second->return_type);
        }
        case ZITH_NODE_BINARY_OP: {
            const SemaType l = sema_expr(ctx, expr->data.kids.a);
            const SemaType r = sema_expr(ctx, expr->data.kids.c);
            if (l == SemaType::String || r == SemaType::String) {
                if (expr->data.list.len == ZITH_TOKEN_PLUS && l == SemaType::String && r == SemaType::String)
                    return SemaType::String;
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, "invalid operands for binary operation");
                return SemaType::Unknown;
            }
            if (l == SemaType::Float || r == SemaType::Float) return SemaType::Float;
            if (l == SemaType::Int && r == SemaType::Int) return SemaType::Int;
            return SemaType::Unknown;
        }
        case ZITH_NODE_UNARY_OP:
            return sema_expr(ctx, expr->data.kids.a);
        default:
            return SemaType::Unknown;
    }
}

static void sema_run(Parser *p, ZithNode *root) {
    if (!root || root->type != ZITH_NODE_PROGRAM) return;
    SemaContext ctx{};
    ctx.p = p;

    auto **decls = static_cast<ZithNode **>(root->data.list.ptr);
    for (size_t i = 0; i < root->data.list.len; ++i) {
        ZithNode *decl = decls[i];
        if (decl && decl->type == ZITH_NODE_FUNC_DECL) {
            auto *fn = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
            ctx.functions[std::string(fn->name, fn->name_len)] = fn;
        }
    }

    for (size_t i = 0; i < root->data.list.len; ++i) {
        ZithNode *decl = decls[i];
        if (!decl || decl->type != ZITH_NODE_FUNC_DECL) continue;
        auto *fn = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
        sema_push_scope(ctx);
        for (size_t pi = 0; pi < fn->param_count; ++pi) {
            auto *param = static_cast<ZithParamPayload *>(fn->params[pi]->data.list.ptr);
            sema_define(ctx, std::string(param->name, param->name_len), sema_type_from_node(param->type_node));
        }
        sema_stmt(ctx, fn->body);
        sema_pop_scope(ctx);
    }
}

static ZithNode *expand_unbody(Parser *parent, ZithNode *node) {
    if (!node) return nullptr;

    if (node->type == ZITH_NODE_UNBODY) {
        const auto *body_tokens = static_cast<const ZithToken *>(node->data.list.ptr);
        const size_t body_len = node->data.list.len;

        auto *tokens = static_cast<ZithToken *>(zith_arena_alloc(parent->arena, sizeof(ZithToken) * (body_len + 1)));
        if (!tokens) return node;
        if (body_len) memcpy(tokens, body_tokens, sizeof(ZithToken) * body_len);
        tokens[body_len] = {{nullptr, 0}, node->loc, ZITH_TOKEN_END, 0};

        Parser inner{};
        parser_init(&inner, parent->arena, parent->source, parent->source_len, parent->filename, {tokens, body_len + 1});
        inner.mode = ZITH_MODE_EXPAND;

        ArenaList<ZithNode *> stmts_b;
        stmts_b.init(parent->arena, 16);
        while (!parser_is_at_end(&inner)) {
            size_t before = inner.pos;
            ZithNode *stmt = parser_parse_statement(&inner);
            if (stmt) stmts_b.push(parent->arena, stmt);
            if (inner.pos == before && !parser_is_at_end(&inner)) parser_advance(&inner);
        }
        size_t count = 0;
        ZithNode **stmts = stmts_b.flatten(parent->arena, &count);

        for (size_t i = 0; i < inner.diags.count; ++i)
            parser_emit_diag(parent, inner.diags.items[i].loc, inner.diags.items[i].severity, inner.diags.items[i].message);

        return zith_ast_make_block(parent->arena, node->loc, stmts, count);
    }

    if (node->type == ZITH_NODE_FUNC_DECL) {
        auto *fn = static_cast<ZithFuncPayload *>(node->data.list.ptr);
        fn->body = expand_unbody(parent, fn->body);
    } else if (node->type == ZITH_NODE_BLOCK || node->type == ZITH_NODE_PROGRAM) {
        auto **items = static_cast<ZithNode **>(node->data.list.ptr);
        for (size_t i = 0; i < node->data.list.len; ++i)
            items[i] = expand_unbody(parent, items[i]);
    } else if (node->type == ZITH_NODE_IF) {
        node->data.kids.b = expand_unbody(parent, node->data.kids.b);
        node->data.kids.c = expand_unbody(parent, node->data.kids.c);
    }

    return node;
}

} // namespace

static ZithNode *run_parser_phase(Parser *p, ZithParserMode mode) {
    p->pos = 0;
    p->panic = false;
    p->had_error = false;
    p->mode = mode;

    if (mode == ZITH_MODE_SCAN) {
        extern void clear_scanned_symbols();
        clear_scanned_symbols();
    }

    ArenaList<ZithNode *> decls_b;
    decls_b.init(p->arena, 16);

    while (!parser_is_at_end(p)) {
        size_t pos_before = p->pos;
        ZithNode *decl = parser_parse_declaration(p);
        if (decl) decls_b.push(p->arena, decl);
        if (p->pos == pos_before && !parser_is_at_end(p)) parser_advance(p);
    }

    size_t count = 0;
    ZithNode **decls = decls_b.flatten(p->arena, &count);
    return zith_ast_make_program(p->arena, decls, count);
}

ZithNode *zith_parse_with_source(ZithArena *arena, const char *source, size_t source_len,
                                         const char *filename, ZithTokenStream tokens) {
    Parser p;
    parser_init(&p, arena, source, source_len, filename, tokens);

    ZithNode *scan_root = run_parser_phase(&p, ZITH_MODE_SCAN);
    p.scan_root = scan_root;

    extern void print_scanned_symbols();
    print_scanned_symbols();

    ZithNode *expanded = expand_unbody(&p, scan_root);
    sema_run(&p, expanded);

    extern void zith_diag_print_all(const ZithDiagList*, const char*, size_t, const char*);
    zith_diag_print_all(&p.diags, source, source_len, filename);

    if (p.had_error) return nullptr;
    return expanded;
}

static ZithArena *g_test_arena = nullptr;

static ZithArena *test_arena_or_init() {
    if (!g_test_arena) g_test_arena = zith_arena_create(1 << 20);
    else zith_arena_reset(g_test_arena);
    return g_test_arena;
}

ZithNode *zith_parse_test(const char *source) {
    if (!source) return nullptr;

    const size_t len = strlen(source);
    ZithArena *arena = test_arena_or_init();
    if (!arena) return nullptr;

    ZithTokenStream tokens = zith_tokenize(arena, source, len);
    if (!tokens.data) return nullptr;

    Parser p;
    parser_init(&p, arena, source, len, "<test>", tokens);

    ZithNode *root = run_parser_phase(&p, ZITH_MODE_SCAN);

    return root;
}

void zith_test_arena_destroy(void) {
    if (g_test_arena) { zith_arena_destroy(g_test_arena); g_test_arena = nullptr; }
}

#ifdef __cplusplus
void ParseResult::reset() {
    if (node) {
        if (g_test_arena) zith_arena_reset(g_test_arena);
        node = nullptr;
    }
}
#endif
