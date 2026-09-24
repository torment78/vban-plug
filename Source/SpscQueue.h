// Copyright (c) 2026 ElkaSoft
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // Intentional cache-line padding between queue cursors.
#endif

namespace vband {
// One producer and one consumer. Never reset cursors while either endpoint is active.
template <typename T, std::size_t Capacity> class SpscQueue {
    static_assert(std::atomic<std::size_t>::is_always_lock_free, "Audio queues require lock-free cursors");
public:
    bool push(const T& value) noexcept {
        const auto w = write.load(std::memory_order_relaxed);
        const auto next = (w + 1) % Capacity;
        if (next == read.load(std::memory_order_acquire)) return false;
        data[w] = value; write.store(next, std::memory_order_release); return true;
    }
    bool pop(T& value) noexcept {
        const auto r = read.load(std::memory_order_relaxed);
        if (r == write.load(std::memory_order_acquire)) return false;
        value = data[r]; read.store((r + 1) % Capacity, std::memory_order_release); return true;
    }
private:
    std::array<T, Capacity> data {};
    alignas(64) std::atomic<std::size_t> read {0};
    alignas(64) std::atomic<std::size_t> write {0};
};
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif