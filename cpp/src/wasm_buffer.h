#ifndef GK_WASM_BUFFER_H
#define GK_WASM_BUFFER_H

#include <cstddef>
#include <cstdint>

// Reserved buffer C API for chunked WASM writes.
// Implementations in wasm_buffer.cc.

extern "C" {

void* gk_reserve_buffer(size_t size);
void* gk_get_buffer_ptr();
size_t gk_get_buffer_size();
size_t gk_get_buffer_used();
void  gk_set_buffer_used(size_t used);
void  gk_release_buffer();
void  gk_reset_memory_stats();
void  gk_get_memory_stats(
    size_t* out_peak_usage,
    size_t* out_current_usage,
    size_t* out_total_allocations,
    size_t* out_total_frees,
    size_t* out_failed_allocations,
    size_t* out_hot_available,
    size_t* out_work_used,
    size_t* out_work_capacity,
    size_t* out_work_peak
);

}  // extern "C"

#endif  // GK_WASM_BUFFER_H
