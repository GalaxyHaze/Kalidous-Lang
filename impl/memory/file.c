// src/utils/file.c
//#include "../../include/Kalidous/utils/i_o_utils.h"
#include <Kalidous/kalidous.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#ifdef _WIN32
    #include <io.h>      // for _access
    #define access _access
    #define F_OK 0
#else
    #include <unistd.h>
#endif

// Helper: check if file exists and is regular
static int is_regular_file(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

bool kalidous_file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

bool kalidous_file_is_regular(const char* path) {
    return is_regular_file(path);
}

size_t kalidous_file_size(const char* path) {
    if (!is_regular_file(path)) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (size_t)st.st_size;
}

// Case-insensitive extension comparison
int kalidous_extension_matches(const char* path, const char* expected_ext) {
    if (!path || !expected_ext) return 0;

    const char* ext = strrchr(path, '.');
    if (!ext) return 0;

    const size_t len1 = strlen(ext);
    const size_t len2 = strlen(expected_ext);
    if (len1 != len2) return 0;

    for (size_t i = 0; i < len1; ++i) {
        if (tolower((unsigned char)ext[i]) != tolower((unsigned char)expected_ext[i]))
            return 0;
    }
    return 1;
}

// Load file into arena
char* kalidous_load_file_to_arena(struct KalidousArena* arena, const char* path, size_t* out_size) {
    if (!arena || !path || !out_size) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        *out_size = 0;
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) goto error;
    const long size = ftell(f);
    if (size < 0) goto error;
    if (fseek(f, 0, SEEK_SET) != 0) goto error;

    if (size == 0) {
        fclose(f);
        *out_size = 0;
        // Return valid empty buffer (optional)
        char* empty = kalidous_arena_alloc(arena, 1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    char* buffer = kalidous_arena_alloc(arena, (size_t)size);
    if (!buffer) goto error;

    const size_t read = fread(buffer, 1, (size_t)size, f);
    fclose(f);

    if (read != (size_t)size) {
        *out_size = 0;
        return NULL;
    }

    *out_size = (size_t)size;
    return buffer;

error:
    fclose(f);
    if (out_size) *out_size = 0;
    return NULL;
}