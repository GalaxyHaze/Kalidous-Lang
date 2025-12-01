//
// Created by diogo on 01/12/25.
//

#ifndef NOVA_PERFECT_HASH_H
#define NOVA_PERFECT_HASH_H
#include <array>
#include <string_view>
#include <cstdint>
#include <cstddef>
#include <stdexcept>

// Simple constexpr FNV-1a 64-bit
consteval uint64_t fnv1a64(std::string_view s) {
    uint64_t hash = 14695981039346656037ull;
    for (char c : s) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}

// next power of two (for small compile-time N)
consteval std::size_t next_pow2(std::size_t v) {
    if (v <= 1) return 1;
    std::size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}

// Primary PerfectHash template
template <std::size_t N, std::size_t TableSize>
struct PerfectHash {
    static_assert(N > 0, "N must be > 0");
    // keys are stored so we can compare during lookup
    std::array<std::string_view, N> keys{};
    std::array<int, TableSize> table{}; // -1 for empty, otherwise index into keys
    uint64_t seed = 0;

    // lookup returns index in [0..N-1], or -1 if not found
    constexpr int operator()(std::string_view q) const noexcept {
        uint64_t h = fnv1a64(q);
        std::size_t idx = static_cast<std::size_t>((h + seed) & (TableSize - 1));
        int ti = table[idx];
        if (ti < 0) return -1;
        // fast check: compare string views
        if (keys[static_cast<std::size_t>(ti)] == q) return ti;
        return -1;
    }
};

// Builder: tries seeds until no collisions
template <std::size_t N>
consteval auto make_perfect_hash(const std::array<std::string_view, N>& keys,
                                 std::size_t table_size = 0,
                                 std::size_t max_seed_tries = 1u << 20)
{
    constexpr std::size_t default_size = next_pow2(N * 2); // usually good
    if (table_size == 0) table_size = default_size;
    if ((table_size & (table_size - 1)) != 0) {
        // enforce power-of-two
        throw std::invalid_argument("table_size must be power of two");
    }
    if (table_size < N) throw std::invalid_argument("table_size must be >= N");

    using PH = PerfectHash<N, static_cast<std::size_t>(table_size)>;
    PH ph{};
    ph.keys = keys;

    // Try seeds
    for (std::size_t s = 1; s <= max_seed_tries; ++s) {
        // init table to -1
        for (std::size_t i = 0; i < table_size; ++i) ph.table[i] = -1;
        bool ok = true;
        for (std::size_t ki = 0; ki < N; ++ki) {
            uint64_t h = fnv1a64(keys[ki]);
            std::size_t idx = static_cast<std::size_t>((h + s) & (table_size - 1));
            if (ph.table[idx] != -1) { ok = false; break; } // collision
            ph.table[idx] = static_cast<int>(ki);
        }
        if (ok) {
            ph.seed = s;
            return ph;
        }
    }
    throw std::runtime_error("Failed to find perfect seed for given keys and table_size");
}

#endif //NOVA_PERFECT_HASH_H