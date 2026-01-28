// include/Nova/arena.h
#ifndef NOVA_ARENA_H
#define NOVA_ARENA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

// Opaque handle
    typedef struct NovaArena NovaArena;

    // Create arena with initial block size (e.g., 64 KiB)
    NovaArena* nova_arena_create(size_t initial_block_size);

    // Allocate `size` bytes (aligned to max_align_t)
    void* nova_arena_alloc(NovaArena* arena, size_t size);

    // Allocate and copy null-terminated string
    char* nova_arena_strdup(NovaArena* arena, const char* str);

    // Reset arena (optional: you may omit if not needed)
    void nova_arena_reset(NovaArena* arena);

    void nova_arena_clean_block(NovaArena* arena);

    // Destroy arena and all blocks
    void nova_arena_destroy(NovaArena* arena);

#ifdef __cplusplus
}
#endif

#endif // NOVA_ARENA_H