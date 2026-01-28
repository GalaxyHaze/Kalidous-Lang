// include/Nova/parse/tokenizer.h
#ifndef NOVA_PARSE_TOKENIZER_H
#define NOVA_PARSE_TOKENIZER_H

#include <stddef.h>
#include "../utils/slice.h"    // defines NovaTokenSlice
#include "Nova/memory/arena_c_functions.h"

#ifdef __cplusplus
#include <string>
#include <vector>
namespace nova::detail {

    struct LexError {
        std::string message;
        NovaInfo info;
    };

    struct NovaArena;

    // Main C++ tokenizer entry point (returns tokens + errors)
    std::vector<NovaToken> tokenize(
        std::string_view src,
        ::NovaArena* arena,
        std::vector<LexError>& errors
    );

} // namespace nova::detail

extern "C" {
#endif

    struct NovaArena;
    NovaTokenSlice nova_tokenize(struct NovaArena* arena, const char* source, size_t source_len);

#ifdef __cplusplus
}
#endif

#endif // NOVA_PARSE_TOKENIZER_H