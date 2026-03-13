#pragma once
#include <cstring>

#include "Kalidous/kalidous.h"

template <typename T>
struct ArenaPage {
    size_t      count;
    size_t      capacity;
    T*          data;
    ArenaPage*  next;
};

template <typename T>
struct ArenaList {
    ArenaPage<T>*  head{};
    ArenaPage<T>*  tail{};
    size_t      page_capacity{};
    size_t      total_items{};

    void init(const KalidousArena* arena, const size_t initial_cap = 64) {
        head = nullptr;
        tail = nullptr;
        total_items = 0;
        page_capacity = initial_cap;
        (void)arena;
    }

    void push(KalidousArena* arena, const T& item) {

        if (!tail || tail->count >= tail->capacity) {

            auto* new_page = static_cast<ArenaPage<T>*>(kalidous_arena_alloc(arena, sizeof(ArenaPage<T>)));
            if (!new_page) return; // OOM

            size_t next_cap = page_capacity;
            if (tail) next_cap = tail->capacity * 2;

            new_page->data = static_cast<T*>(kalidous_arena_alloc(arena, sizeof(T) * next_cap));
            if (!new_page->data) return; // OOM

            new_page->count = 0;
            new_page->capacity = next_cap;
            new_page->next = nullptr;

            if (tail) {
                tail->next = new_page;
                tail = new_page;
            } else {
                head = new_page;
                tail = new_page;
            }
        }

        tail->data[tail->count++] = item;
        total_items++;
    }

    T* flatten(KalidousArena* arena, size_t* out_total_count) {
        if (total_items == 0) {
            *out_total_count = 0;
            auto* dummy = static_cast<T*>(kalidous_arena_alloc(arena, sizeof(T)));
            if(dummy) {
                memset(dummy, 0, sizeof(T));
            }
            return dummy;
        }

        const size_t total_size = sizeof(T) * total_items;
        auto* buf = static_cast<T*>(kalidous_arena_alloc(arena, total_size));
        if (!buf) return nullptr;

        T* cursor = buf;
        ArenaPage<T>* page = head;

        while (page) {
            std::memcpy(cursor, page->data, sizeof(T) * page->count);
            cursor += page->count;
            page = page->next;
        }

        *out_total_count = total_items;
        return buf;
    }
};