#ifndef __C_VMEM_VIRTUAL_MEMORY_ARENA_H__
#define __C_VMEM_VIRTUAL_MEMORY_ARENA_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

namespace ncore
{
    // Arena using virtual memory. Works like a resizable array, but doesn't need to be reallocated and copied.
    // Very useful for implementing memory allocators and containers.
    struct arena_t
    {
        u8*   Mem;              // base address of the memory arena, aligned to page size.
        int_t Pos;              // current byte position in the arena, this is the next available position to allocate from.
        s32   CapacityReserved; // (unit=pages) total size/capacity of the arena
        s32   CapacityCommited; // (unit=pages) number of pages that are commited
        s8    PageSizeShift;    // page size shift, used to compute page size as (1 << PageSizeShift).
        s8    AlignmentShift;   // minimum alignment for allocations, must be a power of two.
        s8    Dummy[6];         // padding to make the struct a power of two size
    };

    enum
    {
        ARENA_DEFAULT_ALIGNMENT_SHIFT = 3, // 8 bytes alignment
        ARENA_DEFAULT_PAGESIZE_SHIFT  = 12 // 4096 bytes page size
    };

    // Initialize the arena system, this must be called before any other arena function
    void ArenasSetup(s32 init_num_arenas = 256, s32 max_num_arenas = 8192, s8 default_alignment_shift = ARENA_DEFAULT_ALIGNMENT_SHIFT, s8 default_page_size_shift = ARENA_DEFAULT_PAGESIZE_SHIFT);
    void ArenasTeardown();

    arena_t* ArenaAlloc(int_t reserved_size_in_bytes, int_t commit_size_in_bytes, s8 alignment_shift = ARENA_DEFAULT_ALIGNMENT_SHIFT, s8 page_size_shift = ARENA_DEFAULT_PAGESIZE_SHIFT);
    bool     ArenaRelease(arena_t* arena);

    // Set the name of the arena, this is used for debugging and logging.
    // Note: These name strings need to have a lifetime longer than the arena itself.
    void        ArenaSetName(arena_t* arena, const char* name);
    const char* ArenaGetName(const arena_t* arena);

    // Current position in the arena, this is the next available position to allocate from.
    int_t ArenaPos(const arena_t* arena);

    // Push requests a memory block of `size_bytes` bytes from the arena.
    void* ArenaPush(arena_t* arena, int_t size_bytes);
    void* ArenaPushZero(arena_t* arena, int_t size_bytes);
    void* ArenaPushAligned(arena_t* arena, int_t size_bytes, s32 alignment);
    void* ArenaPushZeroAligned(arena_t* arena, int_t size_bytes, s32 alignment);

    // Pop releases the last `size_bytes` bytes from the arena.
    void ArenaPopTo(arena_t* arena, int_t position);
    void ArenaPop(arena_t* arena, int_t size_bytes);

    // Clear the arena, this will only reset the commited size when keep_commited_bytes is less than the current commited size.
    void ArenaClear(arena_t* arena, int_t keep_commited_bytes = 0);

    // Commit a specific number of bytes from the arena.
    // If `commited < arena.commited`, this will shrink the usable range.
    // If `commited > arena.commited`, this will expand the usable range.
    void ArenaCommit(arena_t* arena, int_t set_commited_bytes);

    // @returns true if the arena is valid (it was initialized with valid memory and size).
    bool ArenaIsValid(const arena_t* arena);

} // namespace ncore

#endif // __C_VMEM_VIRTUAL_MEMORY_ARENA_H__
