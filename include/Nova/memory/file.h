// include/Nova/utils/file.h
#ifndef NOVA_UTILS_FILE_H
#define NOVA_UTILS_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct NovaArena;

    // --- File system checks (C-friendly versions) ---
    bool nova_file_exists(const char* path);
    bool nova_file_is_regular(const char* path);
    size_t nova_file_size(const char* path); // returns 0 on error

    // --- Extension handling ---
    // Returns true if extension matches (case-insensitive)
    bool nova_extension_matches(const char* path, const char* expected_ext);

    // --- Core: load into arena ---
    // Returns pointer into arena, or NULL on error.
    // Sets *out_size to file size (0 on error).
    char* nova_load_file_to_arena(struct NovaArena* arena, const char* path, size_t* out_size);

#ifdef __cplusplus
}
#endif

#endif // NOVA_UTILS_FILE_H