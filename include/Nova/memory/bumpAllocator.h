//
// Created by diogo on 27/01/26.
//

#ifndef NOVA_BUMPALLOCATOR_H
#define NOVA_BUMPALLOCATOR_H

#include "arena_c_functions.h"
#include <memory>

#include "file.h"

namespace nova {

    class Arena {
    public:
        explicit Arena(size_t initial_block_size = 0);
        ~Arena();
        [[nodiscard]] auto get() const {return handle_.get();}

        Arena(const Arena&) = delete;
        Arena& operator=(const Arena&) = delete;
        Arena(Arena&&) = delete;
        Arena& operator=(Arena&&) = delete;

        [[nodiscard]] void* alloc(size_t size) const { return nova_arena_alloc(handle_.get(), size); }
        char* strdup(const char* str) const { return nova_arena_strdup(handle_.get(), str); }

        // Helper for string_view-based parsing
        std::pair<char*, size_t> load_file(const char* path) const
        {
            size_t size = 0;
            char* data = nova_load_file_to_arena(handle_.get(), path, &size);
            return {data, data ? size : 0};
        }

    private:
        struct Deleter {
            void operator()(NovaArena* a) const { nova_arena_destroy(a); }
        };
        std::unique_ptr<NovaArena, Deleter> handle_;
    };


    inline Arena::Arena(const size_t initial_block_size)
        : handle_(nova_arena_create(initial_block_size)) {
        if (!handle_) throw std::bad_alloc{};
    }

    inline Arena::~Arena() = default;

} // namespace Nova

#endif //NOVA_BUMPALLOCATOR_H