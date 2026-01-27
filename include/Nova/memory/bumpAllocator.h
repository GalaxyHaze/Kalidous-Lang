#ifndef NOVA_BUMPALLOCATOR_H
#define NOVA_BUMPALLOCATOR_H

// 1. Guard this header so C files never see this C++ code
#ifdef __cplusplus

#include <memory>
#include <utility>

// Include the C declarations here (which are now safe)
#include "arena_c_functions.h"
// You might need to adjust the path depending on your folder structure
// e.g., #include "Nova/memory/arena_c_functions.h"

namespace nova {

    // The opaque struct is defined in the .c file, but we can forward declare it
    // or rely on the fact we included the C header above.
    // Since we included arena_c_functions.h, NovaArena is already known to the compiler.

    class Arena {
    public:
        explicit Arena(size_t initial_block_size = 0);
        ~Arena();

        // Disable copies, enable moves (optional, but usually good for allocators)
        Arena(const Arena&) = delete;
        Arena& operator=(const Arena&) = delete;
        Arena(Arena&&) noexcept = default;
        Arena& operator=(Arena&&) noexcept = default;
        template<class T, class... Args>
        T* create(Args&&... args)
        {
            auto ptr = nova_arena_alloc(handle_.get(), sizeof(T));
            if (!ptr) throw std::bad_alloc{};
            new (ptr) T(std::forward<Args>(args)...);
            return ptr;
        }

        [[nodiscard]] auto get() const { return handle_.get(); }

        [[nodiscard]] void* alloc(size_t size) const {
            return nova_arena_alloc(handle_.get(), size);
        }

        char* strdup(const char* str) const {
            return nova_arena_strdup(handle_.get(), str);
        }

        // Helper for file loading - ensure nova_load_file_to_arena is declared in your C header
        std::pair<char*, size_t> load_file(const char* path) const {
            size_t size = 0;
            char* data = nova_load_file_to_arena(handle_.get(), path, &size);
            return {data, data ? size : 0};
        }

    private:
        struct Deleter {
            void operator()(NovaArena* a) const {
                nova_arena_destroy(a);
            }
        };
        std::unique_ptr<NovaArena, Deleter> handle_;
    };

    inline Arena::Arena(const size_t initial_block_size)
        : handle_(nova_arena_create(initial_block_size)) {
        if (!handle_) throw std::bad_alloc{};
    }

    inline Arena::~Arena() = default;

} // namespace nova

#endif // __cplusplus
#endif // NOVA_BUMPALLOCATOR_H