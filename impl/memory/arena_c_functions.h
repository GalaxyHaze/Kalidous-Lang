#pragma once

#include <zith/zith.hpp>

#ifdef __cplusplus
extern "C" {
#endif

ZithArena *zith_arena_create(size_t initial_block_size);
void zith_arena_destroy(ZithArena *arena);
void *zith_arena_alloc(ZithArena *arena, size_t size);
void zith_arena_reset(ZithArena *arena);
size_t zith_arena_used(const ZithArena *arena);

#ifdef __cplusplus
}
#endif