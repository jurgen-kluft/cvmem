#ifndef __C_VMEM_VIRTUAL_MEMORY_ARENA_H__
#define __C_VMEM_VIRTUAL_MEMORY_ARENA_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "cvmem/c_virtual_memory.h"

namespace ncore
{
    // Arena of virtual memory. Works like a resizable array, but doesn't need to be reallocated and copied.
    // Very useful for implementing memory allocators and containers.
    struct vmem_arena_t
    {
        u8*         mem;        // Base address of the memory arena. Aligned to page size. Points to memory allocated with `vmem_alloc`.
        vmem_size_t size_bytes; // Total size/capacity of the arena.
        vmem_size_t commited;   // Number of bytes from range [mem...mem+size_bytes] which are commited and usable.
    };

    // Initialize the arena with an existing memory block, which you manage on your own.
    // Note: when using this, use `vmem_dealloc` on your own, don't call `vmem_arena_deinit_dealloc`!
    // @param mem: pointer returned by `vmem_alloc`, or shifted by N bytes (you can sub-allocate one memory allocation).
    vmem_arena_t vmem_arena_init(void* mem, vmem_size_t size_bytes);

    // Initialize an arena and allocate memory of size `size_bytes`.
    // Use `vmem_arena_deinit_dealloc` to free the memory.
    vmem_arena_t vmem_arena_init_alloc(vmem_size_t size_bytes);

    // De-initialize an arena initialized with `vmem_arena_init_alloc`.
    // Frees the arena memory using `vmem_dealloc`.
    vmem_result_t vmem_arena_deinit_dealloc(vmem_arena_t* arena);

    // Commit a specific number of bytes from the arena.
    // If `commited < arena.commited`, this will shrink the usable range.
    // If `commited > arena.commited`, this will expand the usable range.
    vmem_result_t vmem_arena_set_commited(vmem_arena_t* arena, vmem_size_t commited);

    // @returns true if the arena is valid (it was initialized with valid memory and size).
    static inline vmem_result_t vmem_arena_is_valid(const vmem_arena_t* arena) { return (arena != nullptr && arena->mem != 0 && arena->size_bytes > 0) ? vmem_result_t{vmem_result_t::Success} : vmem_result_t{vmem_result_t::Error}; }

    // @returns number of bytes which are physically used for a given size bytes (or commited bytes).
    static inline vmem_size_t vmem_arena_calc_bytes_used_for_size(const vmem_size_t size_bytes) { return vmem_align_forward(size_bytes, vmem_get_page_size()); }

} // namespace ncore

#endif // __C_VMEM_VIRTUAL_MEMORY_ARENA_H__
