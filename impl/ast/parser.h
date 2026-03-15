// impl/parser/kalidous_parser.h — Parser interface for Kalidous
//
// kalidous.h declares the public entry point:
//   KalidousNode* kalidous_parse(KalidousArena*, KalidousTokenStream);
//
// This header exposes the Parser state and the sub-parsers so that:
//   - kalidous_parser.cpp can implement them
//   - future passes (sema, repl incremental parse) can drive the parser
//     without going through the full kalidous_parse() pipeline
//   - tests can call individual parse_* functions directly
#pragma once

#include <kalidous/kalidous.h>
#include "../utils/debug.h"
#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Parser state
// ============================================================================

typedef struct Parser {
    KalidousArena*       arena;
    const KalidousToken* tokens;
    size_t               count;
    size_t               pos;
    bool                 had_error;

    // Current function context — used by sema-lite checks in the parser:
    //   - yield only valid when fn_kind == ASYNC
    //   - goto/marker only valid when fn_kind == NORETURN or FLOWING
    //   - recurse only valid when inside_fn == true
    KalidousFnKind       fn_kind;
    bool                 inside_fn;

    // Active group visibility modifier — set by  public: / private: / protected:
    // Inherited by each item until the next group modifier or end of scope.
    // Default: KALIDOUS_VIS_PRIVATE
    KalidousVisibility   current_visibility;
} Parser;

// ============================================================================
// Parser init
// ============================================================================

void parser_init(Parser* p, KalidousArena* arena, KalidousTokenStream tokens);

// ============================================================================
// Token navigation (used by ast.cpp debug helpers and future passes)
// ============================================================================

const KalidousToken* parser_peek       (const Parser* p);
const KalidousToken* parser_peek_ahead (const Parser* p, size_t offset);
const KalidousToken* parser_advance    (Parser* p);
bool                 parser_check      (const Parser* p, KalidousTokenType type);
bool                 parser_match      (Parser* p, KalidousTokenType type);
const KalidousToken* parser_expect     (Parser* p, KalidousTokenType type, const char* msg);
bool                 parser_is_at_end  (const Parser* p);

// ============================================================================
// Sub-parsers — exposed so tests and future passes can call them directly
// ============================================================================

KalidousNode* parser_parse_declaration (Parser* p);
KalidousNode* parser_parse_statement   (Parser* p);
KalidousNode* parser_parse_expression  (Parser* p);
KalidousNode* parser_parse_type        (Parser* p);
KalidousNode* parser_parse_block       (Parser* p);

#ifdef __cplusplus
}
#endif