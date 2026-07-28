#ifndef GK_WASM_BUFFER_H
#define GK_WASM_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "memory_pool.h"

namespace gk {
namespace {

// Reserved buffer for chunked WASM writes.
// Not thread-safe; single WASM caller.
constexpr size_t kReservedBufferSize = 8u * 1024u * 1024u;  // 8MB

std::byte* g_reservedBuffer = nullptr;
size_t g_reservedBufferSize = 0;
size_t g_reservedBufferUsed = 0;

}  // anonymous namespace
}  // namespace gk

extern "C" {

static void releaseInternal() {
    if (gk::g_reservedBuffer) {
        gk::memoryTracker().trackExternalFree(gk::g_reservedBufferSize);
        std::free(gk::g_reservedBuffer);
        gk::g_reservedBuffer = nullptr;
        gk::g_reservedBufferSize = 0;
        gk::g_reservedBufferUsed = 0;
    }
}

void* gk_reserve_buffer(size_t size) {
    releaseInternal();
    if (size == 0) {
        size = gk::kReservedBufferSize;
    }
    auto* buf = static_cast<std::byte*>(std::aligned_alloc(alignof(std::max_align_t), size));
    if (buf == nullptr) {
        gk::trackFailedAllocation();
        return nullptr;
    }
    gk::g_reservedBuffer = buf;
    gk::g_reservedBufferSize = size;
    gk::g_reservedBufferUsed = 0;
    gk::memoryTracker().trackExternalAlloc(size);
    return buf;
}

void* gk_get_buffer_ptr() {
    return gk::g_reservedBuffer;
}

size_t gk_get_buffer_size() {
    return gk::g_reservedBufferSize;
}

size_t gk_get_buffer_used() {
    return gk::g_reservedBufferUsed;
}

void gk_set_buffer_used(size_t used) {
    if (used <= gk::g_reservedBufferSize) {
        gk::g_reservedBufferUsed = used;
    }
}

void gk_release_buffer() {
    releaseInternal();
}

void gk_reset_memory_stats() {
    gk::resetMemoryStats();
}

void gk_get_memory_stats(
    size_t* out_peak_usage,
    size_t* out_current_usage,
    size_t* out_total_allocations,
    size_t* out_total_frees,
    size_t* out_failed_allocations,
    size_t* out_hot_available,
    size_t* out_work_used,
    size_t* out_work_capacity,
    size_t* out_work_peak
) {
    auto stats = gk::getMemoryStats();
    if (out_peak_usage)        *out_peak_usage = stats.peak_usage;
    if (out_current_usage)     *out_current_usage = stats.current_usage;
    if (out_total_allocations) *out_total_allocations = stats.total_allocations;
    if (out_total_frees)       *out_total_frees = stats.total_frees;
    if (out_failed_allocations) *out_failed_allocations = stats.failed_allocations;
    if (out_hot_available)     *out_hot_available = stats.hot_available;
    if (out_work_used)         *out_work_used = stats.work_used;
    if (out_work_capacity)     *out_work_capacity = stats.work_capacity;
    if (out_work_peak)         *out_work_peak = stats.work_peak;
}

}  // extern "C"

#endif  // GK_WASM_BUFFER_H
