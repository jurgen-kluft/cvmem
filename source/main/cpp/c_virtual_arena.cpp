#include "ccore/c_target.h"
#include "ccore/c_allocator.h"
#include "ccore/c_debug.h"
#include "ccore/c_error.h"
#include "ccore/c_math.h"
#include "ccore/c_memory.h"

#include "cvmem/c_virtual_memory.h"
#include "cvmem/c_virtual_arena.h"

#if !defined(TARGET_DEBUG)
#    define VMEM_NO_ERROR_CHECKING
#    define VMEM_NO_ERROR_MESSAGES
#endif

namespace ncore
{
    enum eArenaErrors
    {
        cArenaErrorReserveMemory  = -1, // Failed to reserve memory for the arena.
        cArenaErrorCommitMemory   = -2, // Failed to commit memory for the arena.
        cArenaErrorNotInitialized = -3, // Arena has not been initialized?
        cArenaErrorGrow           = -4, // Failed to grow the arena.
        cArenaErrorShrink         = -5, // Failed to shrink the arena.
        cArenaErrorAlignmentShift = -6, // Alignment shift must be between 0 and 16.
        cArenaErrorPageSizeShift  = -7, // Page size shift must be between 12 and 20.
        cArenaErrorMaxErrors      = 10,
    };

    static s64 gArenaErrorBase = -1;
    const char* gArenaErrorToString(error_t error)
    {
        error = error - gArenaErrorBase;
        switch (error)
        {
            case eArenaErrors::cArenaErrorReserveMemory: return "failed to reserve memory for the arena.";
            case eArenaErrors::cArenaErrorCommitMemory: return "failed to commit memory for the arena.";
            case eArenaErrors::cArenaErrorNotInitialized: return "arena has not been initialized?";
            case eArenaErrors::cArenaErrorGrow: return "failed to grow the arena.";
            case eArenaErrors::cArenaErrorShrink: return "failed to shrink the arena.";
            case eArenaErrors::cArenaErrorAlignmentShift: return "alignment shift must be between 0 and 16.";
            case eArenaErrors::cArenaErrorPageSizeShift: return "page size shift must be between 12 and 20.";
            default: return "unknown arena error";
        }
    }

#if !defined(VMEM_NO_ERROR_CHECKING)
#    if !defined(VMEM_NO_ERROR_MESSAGES)
    static void _write_error(eArenaErrors error)
    {
        const error_t e = gArenaErrorBase + error;
        nerror::error(e);
        ASSERTS(false, gArenaErrorToString(e));
    }
#    else
    static void _write_error(error_t error) {}
#    endif

    static bool check(bool cond, eArenaErrors error)
    {
        if (!cond)
            _write_error(error);
        return cond;
    }

#else
    static void _write_error(eArenaErrors error)
    {
        const error_t e = gArenaErrorBase + error;
        nerror::error(e);
    }

    static bool check(bool cond, eArenaErrors error)
    {
        if (!cond)
            _write_error(error);
        return true;
    }
#endif

    // arena_t (32 bytes) + zarena_t (32 bytes) = 64 bytes
    struct zarena_t
    {
        arena_t     Arena;
        const char* Name;
        zarena_t*   Next;
        void*       Padding0;
        void*       Padding1;
    };

    static arena_t   g_arena_array             = {0};
    static s32       g_arena_max_index         = 0;
    static s32       g_arena_free_index        = 0;
    static zarena_t* g_arena_free_head         = nullptr;
    static s8        g_default_alignment_shift = 3;  // 8 bytes
    static s8        g_default_page_size_shift = 12; // 4096 bytes

    static inline s32 gArenaPtrToIndex(const zarena_t* arena)
    {
        if (arena == nullptr)
            return -1;
        return (s32)(arena - (const zarena_t*)g_arena_array.Mem) / sizeof(zarena_t);
    }

    static inline zarena_t* gArenaIndexToPtr(s32 index)
    {
        if (index < 0 || index >= g_arena_free_index)
            return nullptr;
        zarena_t* arenaArray = (zarena_t*)g_arena_array.Mem;
        return &arenaArray[index];
    }

    void ArenasSetup(s32 init_num_arenas, s32 max_num_arenas, s8 default_alignment_shift, s8 default_page_size_shift)
    {
        if (g_arena_array.Mem == nullptr)
        {
            gArenaErrorBase = nerror::insert_handler(gArenaErrorToString, cArenaErrorMaxErrors);

            const u32 page_size       = nvmem::query_page_size(); // Initialize the page size query
            const s8  page_size_shift = math::g_ilog2(page_size);

            default_page_size_shift = math::g_clamp<s8>(default_page_size_shift, 0, page_size_shift);

            g_arena_array.Mem              = (u8*)nvmem::alloc(max_num_arenas * sizeof(zarena_t));
            g_arena_array.Pos              = 0;
            g_arena_array.CapacityReserved = math::g_alignUp(max_num_arenas * sizeof(zarena_t), page_size) >> page_size_shift;  // Total size/capacity of the arena in pages.
            g_arena_array.CapacityCommited = math::g_alignUp(init_num_arenas * sizeof(zarena_t), page_size) >> page_size_shift; // Initially no pages are commited
            g_arena_array.PageSizeShift    = default_page_size_shift;                                                           // Page size shift, used to compute page size as (1 << PageSizeShift).
            g_arena_array.AlignmentShift   = default_alignment_shift;                                                           // Minimum alignment for allocations, must be a power of two.
            nvmem::commit(g_arena_array.Mem, (init_num_arenas << g_arena_array.PageSizeShift));                                 // Commit the first page
        }
    }

    void ArenasTeardown()
    {
        if (g_arena_array.Mem != nullptr)
        {
            nvmem::decommit(g_arena_array.Mem, g_arena_array.CapacityCommited << g_arena_array.PageSizeShift); // Decommit the commited memory
            nvmem::release(g_arena_array.Mem, g_arena_array.CapacityReserved << g_arena_array.PageSizeShift);  // Release the reserved memory
            g_arena_array.Mem         = nullptr;                                                               // Reset the arena array
            g_arena_max_index         = 0;
            g_arena_free_index        = 0;
            g_arena_free_head         = nullptr; // Reset the free list
            g_default_alignment_shift = 3;       // Reset to default alignment shift
            g_default_page_size_shift = 12;      // Reset to default page size shift
        }
    }

    void ArenaSetName(arena_t* arena, const char* name)
    {
        zarena_t* zarena = (zarena_t*)arena;
        zarena->Name     = name;
    }

    const char* ArenaGetName(const arena_t* arena)
    {
        zarena_t* zarena = (zarena_t*)arena;
        return zarena->Name;
    }

    arena_t* ArenaAlloc(int_t reserved_size_in_bytes, int_t commit_size_in_bytes, s8 alignment_shift, s8 page_size_shift)
    {
        check(alignment_shift >= 0 && alignment_shift <= 16, cArenaErrorAlignmentShift);
        check(page_size_shift >= 12 && page_size_shift <= 20, cArenaErrorPageSizeShift);

        // align the reserved size to the page size
        int_t reserved_size_in_pages = (reserved_size_in_bytes + ((int_t)1 << page_size_shift) - 1) >> page_size_shift;
        reserved_size_in_bytes       = reserved_size_in_pages << page_size_shift; // Align to page size
        void* reserved_mem_ptr       = nullptr;
        if (!nvmem::reserve((u64)reserved_size_in_bytes, nvmem::nprotect::ReadWrite, reserved_mem_ptr))
        {
            _write_error(cArenaErrorReserveMemory);
            return nullptr; // Reserve memory for the arena failed
        }
        int_t commit_size_in_pages = (commit_size_in_bytes + ((int_t)1 << page_size_shift) - 1) >> page_size_shift;
        commit_size_in_bytes       = commit_size_in_pages << page_size_shift; // Align to page size
        if (!nvmem::commit(reserved_mem_ptr, commit_size_in_bytes))
        {
            _write_error(cArenaErrorCommitMemory);
            nvmem::release(reserved_mem_ptr, reserved_size_in_bytes); // Release the reserved memory
            return nullptr;                                           // Commit memory for the arena failed
        }

        s32 arenaIndex = -1;
        if (g_arena_free_head != nullptr)
        {
            arenaIndex        = gArenaPtrToIndex(g_arena_free_head);
            g_arena_free_head = g_arena_free_head->Next; // Pop the first arena from the free list
        }
        else if (g_arena_free_index < g_arena_max_index)
        {
            arenaIndex = g_arena_free_index++;
        }
        else
        {
            // Commit more pages to the arena array
            s32 newMaxIndex = g_arena_max_index + 256; // Increase by 256 arenas
            nvmem::commit(g_arena_array.Mem + (g_arena_max_index * sizeof(zarena_t)),
                          256 * sizeof(zarena_t)); // Commit memory for the new arenas
        }

        zarena_t* arenaArray = (zarena_t*)g_arena_array.Mem;

        zarena_t* zarena = &arenaArray[arenaIndex];
        zarena->Name     = "none";

        arena_t* arena          = &zarena->Arena;
        arena->Mem              = (u8*)reserved_mem_ptr;
        arena->Pos              = 0;
        arena->CapacityReserved = reserved_size_in_pages;
        arena->CapacityCommited = commit_size_in_pages;
        arena->PageSizeShift    = page_size_shift;
        arena->AlignmentShift   = alignment_shift;

        return arena;
    }

    bool ArenaRelease(arena_t* arena)
    {
        if (arena == nullptr)
            return true;

        // TODO Release commited and reserved memory

        // Reset the arena
        arena->Mem              = nullptr;
        arena->CapacityReserved = 0;
        arena->CapacityCommited = 0;
        arena->PageSizeShift    = g_default_page_size_shift;
        arena->AlignmentShift   = g_default_alignment_shift;

        // Put it back to the free list
        zarena_t* zarena  = (zarena_t*)arena;  // Cast arena to zarena_t
        zarena->Next      = g_arena_free_head; // Link the arena to the head of the free list
        g_arena_free_head = zarena;            // Update the head of the free list

        return true;
    }

    int_t ArenaPos(const arena_t* arena)
    {
        if (arena == nullptr)
            return -1; // Invalid arena pointer
        return arena->Pos;
    }

    static bool ArenaSetCapacity(arena_t* arena, int_t newCapacityInBytes)
    {
        // calculate the new size in pages
        s32 const newSizeInPages = (newCapacityInBytes + ((int_t)1 << arena->PageSizeShift) - 1) >> arena->PageSizeShift;

        if (arena->Mem == nullptr)
        {
            _write_error(cArenaErrorNotInitialized);
            return false; // Arena memory is not allocated
        }

        if (newSizeInPages > arena->CapacityCommited)
        {
            if (newSizeInPages > arena->CapacityReserved)
            {
                // we cannot expand the arena beyond its reserved capacity
                return false;
            }

            // if we are expanding the arena, we need to commit more memory
            const int_t currentSizeInBytes = arena->CapacityCommited << arena->PageSizeShift;
            const int_t newSizeInBytes     = newSizeInPages << arena->PageSizeShift;

            if (!nvmem::commit(arena->Mem + currentSizeInBytes, newSizeInBytes - currentSizeInBytes))
            {
                return false;
            }

            // update the arena since we are expanding the commited size
            arena->CapacityCommited = newSizeInPages;
        }
        else if (newSizeInPages < arena->CapacityCommited)
        {
            // if we are shrinking the arena, we need to decommit the excess memory
            const int_t currentSizeInBytes = arena->CapacityCommited << arena->PageSizeShift;
            const int_t newSizeInBytes     = newSizeInPages << arena->PageSizeShift;

            if (!nvmem::decommit(arena->Mem + newSizeInBytes, currentSizeInBytes - newSizeInBytes))
            {
                return false;
            }

            // update the commited size
            arena->CapacityCommited = newSizeInPages;
        }

        return true;
    }

    void* ArenaPush(arena_t* arena, int_t size_bytes)
    {
        if ((arena->Pos + size_bytes) > ((int_t)arena->CapacityCommited << arena->PageSizeShift))
        {
            const int_t newCapacityInBytes = (arena->Pos + size_bytes + ((int_t)1 << arena->PageSizeShift) - 1) & ~(((int_t)1 << arena->PageSizeShift) - 1);
            if (!ArenaSetCapacity(arena, newCapacityInBytes))
            {
                _write_error(cArenaErrorGrow);
                return nullptr; // Failed to grow the arena
            }
        }

        const int_t pos = arena->Pos;
        arena->Pos += size_bytes;  // Move the position forward
        return (arena->Mem + pos); // Return the pointer to the allocated memory
    }

    void* ArenaPushZero(arena_t* arena, int_t size_bytes)
    {
        void* ptr = ArenaPush(arena, size_bytes);
        if (ptr != nullptr)
        {
            nmem::memset(ptr, 0, size_bytes); // Zero out the allocated memory
        }
        return ptr;
    }

    void* ArenaPushAligned(arena_t* arena, int_t size_bytes, s32 alignment)
    {
        // align the position of the arena to the specified alignment
        const int_t alignedPos = (arena->Pos + ((int_t)alignment - 1)) & ~((int_t)alignment - 1);
        if ((alignedPos + size_bytes) > ((int_t)arena->CapacityCommited << arena->PageSizeShift))
        {
            if (!ArenaSetCapacity(arena, alignedPos + size_bytes))
            {
                _write_error(cArenaErrorGrow);
                return nullptr; // Failed to grow the arena
            }
        }
        return ArenaPush(arena, size_bytes);
    }

    void* ArenaPushZeroAligned(arena_t* arena, int_t size_bytes, s32 alignment)
    {
        void* ptr = ArenaPushAligned(arena, size_bytes, alignment);
        if (ptr != nullptr)
        {
            nmem::memset(ptr, 0, size_bytes); // Zero out the allocated memory
        }
        return ptr;
    }

    void ArenaPopTo(arena_t* arena, int_t position)
    {
        position   = math::g_clamp<int_t>(position, 0, arena->Pos); // Ensure position is within valid range
        arena->Pos = position;
    }

    void ArenaPop(arena_t* arena, int_t size_bytes)
    {
        size_bytes = math::g_clamp<int_t>(size_bytes, 0, arena->Pos); // Ensure size_bytes is within valid range
        arena->Pos -= size_bytes;
    }

    void ArenaClear(arena_t* arena, int_t keep_commited_bytes)
    {
        int_t keep_commited_pages = (keep_commited_bytes + ((int_t)1 << arena->PageSizeShift) - 1) >> arena->PageSizeShift;
        keep_commited_pages       = math::g_clamp<int_t>(keep_commited_pages, 0, arena->CapacityCommited);

        // Reset the position to 0
        arena->Pos = 0;

        // If keep_commited_bytes is greater than 0, we need to keep the commited memory
        if (keep_commited_pages < arena->CapacityCommited)
        {
            // If we are shrinking the arena, we need to decommit the excess memory
            const int_t currentSizeInBytes = arena->CapacityCommited << arena->PageSizeShift;
            const int_t newSizeInBytes     = keep_commited_pages << arena->PageSizeShift;

            if (newSizeInBytes < currentSizeInBytes)
            {
                if (!nvmem::decommit(arena->Mem + newSizeInBytes, currentSizeInBytes - newSizeInBytes))
                {
                    _write_error(cArenaErrorShrink);
                }
            }

            // Update the commited size
            arena->CapacityCommited = keep_commited_pages;
        }
    }

    void ArenaCommit(arena_t* arena, int_t set_commited_bytes)
    {
        if (arena == nullptr)
            return;

        ArenaSetCapacity(arena, set_commited_bytes);
    }

    bool ArenaIsValid(const arena_t* arena)
    {
        if (arena == nullptr)
            return false;

        // Check if the arena has a valid memory pointer and capacity
        if (arena->Mem == nullptr || arena->CapacityReserved <= 0 || arena->CapacityCommited <= 0)
            return false;

        // Check if the position is within the commited range
        if (arena->Pos < 0 || arena->Pos > (arena->CapacityCommited << arena->PageSizeShift))
            return false;

        return true;
    }
} // namespace ncore
