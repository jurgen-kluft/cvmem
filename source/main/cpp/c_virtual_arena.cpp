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
        cArenaErrorReserveMemory  = 0, // Failed to reserve memory for the arena.
        cArenaErrorCommitMemory   = 1, // Failed to commit memory for the arena.
        cArenaErrorNotInitialized = 2, // Arena has not been initialized?
        cArenaErrorGrow           = 3, // Failed to grow the arena.
        cArenaErrorShrink         = 4, // Failed to shrink the arena.
        cArenaErrorRelease        = 5, // Failed to release the arena.
        cArenaErrorAlignmentShift = 6, // Alignment shift must be between 0 and 16.
        cArenaErrorPageSizeShift  = 7, // Page size shift must be between 12 and 20.
        cArenaErrorMaxErrors      = 10,
    };

    static s64  gArenaErrorBase = 0;
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
            case eArenaErrors::cArenaErrorRelease: return "failed to release the arena.";
            case eArenaErrors::cArenaErrorAlignmentShift: return "alignment shift must be between 0 and 16.";
            case eArenaErrors::cArenaErrorPageSizeShift: return "page size shift must be between 12 and 20.";
            default: return "unknown arena error";
        }
    }

#if !defined(VMEM_NO_ERROR_CHECKING)
#    if !defined(VMEM_NO_ERROR_MESSAGES)
    static void arena_error(eArenaErrors error)
    {
        const error_t e = gArenaErrorBase + error;
        nerror::error(e);
        ASSERTS(false, gArenaErrorToString(e));
    }
#    else
    static void arena_error(error_t error) {}
#    endif

    static bool check(bool cond, eArenaErrors error)
    {
        if (!cond)
            arena_error(error);
        return cond;
    }

#else
    static void arena_error(eArenaErrors error)
    {
        const error_t e = gArenaErrorBase + error;
        nerror::error(e);
    }

    static bool check(bool cond, eArenaErrors error)
    {
        if (!cond)
            arena_error(error);
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

    static inline int_t CommittedInBytes(arena_t const& Arena)
    {
        return Arena.CapacityCommited << Arena.PageSizeShift; // Capacity in bytes
    }
    static inline int_t ReservedInBytes(arena_t const& Arena)
    {
        return Arena.CapacityReserved << Arena.PageSizeShift; // Capacity in bytes
    }
    static inline int_t AlignToPageSize(arena_t const& Arena, int_t size) { return math::g_alignUp<int_t>(size, (int_t)1 << Arena.PageSizeShift); }
    static inline int_t NumBytesToPages(arena_t const& Arena, int_t sizeInByes) { return math::g_alignUp<int_t>(sizeInByes, (int_t)1 << Arena.PageSizeShift) >> Arena.PageSizeShift; }
    static inline int_t NumPagesToBytes(arena_t const& Arena, int_t numPages)
    {
        return numPages << Arena.PageSizeShift; // Convert pages to bytes
    }

    struct zarena_system_t
    {
        void reset()
        {
            m_array      = {0};
            m_arena_max_index  = 0;
            m_arena_free_index = 0;
            m_arena_free_head  = nullptr;
        }

        arena_t   m_array;
        s32       m_arena_cap_index;
        s32       m_arena_max_index;
        s32       m_arena_free_index;
        zarena_t* m_arena_free_head;
    };

    static zarena_system_t sArenas = {0};

    static inline s32 gArenaPtrToIndex(const zarena_t* arena)
    {
        if (arena == nullptr)
            return -1;
        return (s32)(arena - (const zarena_t*)sArenas.m_array.Mem) / sizeof(zarena_t);
    }

    static inline zarena_t* gArenaIndexToPtr(s32 index)
    {
        if (index < 0 || index >= sArenas.m_arena_free_index)
            return nullptr;
        zarena_t* arenaArray = (zarena_t*)sArenas.m_array.Mem;
        return &arenaArray[index];
    }

    void ArenasSetup(s32 init_num_arenas, s32 max_num_arenas, s8 default_alignment_shift, s8 default_page_size_shift)
    {
        if (sArenas.m_array.Mem == nullptr)
        {
            nvmem::initialize();

            gArenaErrorBase = nerror::insert_handler(gArenaErrorToString, cArenaErrorMaxErrors);

            {
                const u32 page_size       = nvmem::query_page_size(); // Initialize the page size query
                const s8  page_size_shift = math::g_ilog2(page_size);
                default_page_size_shift   = math::g_clamp<s8>(default_page_size_shift, page_size_shift, 20);
                default_alignment_shift   = math::g_clamp<s8>(default_alignment_shift, 2, 16);
            }

            sArenas.m_array.Mem            = nullptr;
            sArenas.m_array.Pos            = 0;
            sArenas.m_array.PageSizeShift  = default_page_size_shift; // Page size shift, used to compute page size as (1 << PageSizeShift).
            sArenas.m_array.AlignmentShift = default_alignment_shift; // Minimum alignment for allocations, must be a power of two.

            const s32   reserve_num_pages = NumBytesToPages(sArenas.m_array, max_num_arenas * sizeof(zarena_t));
            const int_t reserve_num_bytes = NumPagesToBytes(sArenas.m_array, reserve_num_pages);
            void*       arena_mem_ptr     = nullptr;
            if (!nvmem::reserve(reserve_num_bytes, nvmem::nprotect::ReadWrite, arena_mem_ptr))
            {
                arena_error(cArenaErrorReserveMemory);
                return;
            }

            const s32   commit_num_pages = NumBytesToPages(sArenas.m_array, init_num_arenas * sizeof(zarena_t));
            const int_t commit_num_bytes = NumPagesToBytes(sArenas.m_array, commit_num_pages);
            if (!nvmem::commit(arena_mem_ptr, commit_num_bytes))
            {
                nvmem::release(arena_mem_ptr, max_num_arenas * sizeof(zarena_t));
                arena_error(cArenaErrorCommitMemory);
                return;
            }

            sArenas.m_array.CapacityReserved = reserve_num_pages;
            sArenas.m_array.CapacityCommited = commit_num_pages;
            sArenas.m_array.Mem              = (u8*)arena_mem_ptr;
            sArenas.m_arena_cap_index              = NumPagesToBytes(sArenas.m_array, sArenas.m_array.CapacityReserved) / sizeof(zarena_t);
            sArenas.m_arena_max_index              = init_num_arenas; // Maximum number of arena objects we can have
            sArenas.m_arena_free_index             = 0;               // Index of the next free arena in the array
            sArenas.m_arena_free_head              = nullptr;         // Head of the free list of arenas
        }
    }

    void ArenasTeardown()
    {
        if (sArenas.m_array.Mem != nullptr)
        {
            nvmem::decommit(sArenas.m_array.Mem, NumPagesToBytes(sArenas.m_array, sArenas.m_array.CapacityCommited));
            nvmem::release(sArenas.m_array.Mem, NumPagesToBytes(sArenas.m_array, sArenas.m_array.CapacityReserved));
            sArenas.reset();
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
        arena_t arena;
        arena.Mem              = nullptr;
        arena.Pos              = 0;
        arena.CapacityCommited = 0;
        arena.CapacityReserved = 0;
        arena.PageSizeShift    = math::g_clamp<s8>(page_size_shift, sArenas.m_array.PageSizeShift, 20);
        arena.AlignmentShift   = math::g_clamp<s8>(alignment_shift, sArenas.m_array.AlignmentShift, 16);

        // align the reserved size to the page size
        const int_t reserved_pages   = NumBytesToPages(arena, reserved_size_in_bytes);
        const int_t reserved_bytes   = NumPagesToBytes(arena, reserved_pages);
        void*       reserved_mem_ptr = nullptr;
        if (!nvmem::reserve((u64)reserved_bytes, nvmem::nprotect::ReadWrite, reserved_mem_ptr))
        {
            arena_error(cArenaErrorReserveMemory);
            return nullptr; // Reserve memory for the arena failed
        }
        const int_t commit_pages = NumBytesToPages(arena, commit_size_in_bytes);
        const int_t commit_bytes = NumPagesToBytes(arena, commit_pages);
        if (!nvmem::commit(reserved_mem_ptr, commit_bytes))
        {
            arena_error(cArenaErrorCommitMemory);
            nvmem::release(reserved_mem_ptr, reserved_size_in_bytes); // Release the reserved memory
            return nullptr;                                           // Commit memory for the arena failed
        }

        arena.Mem              = (u8*)reserved_mem_ptr; // Set the memory pointer to the reserved memory
        arena.CapacityReserved = reserved_pages;        // Set the reserved capacity in pages
        arena.CapacityCommited = commit_pages;          // Set the commited capacity in pages

        zarena_t* zarena = nullptr;
        if (sArenas.m_arena_free_head != nullptr)
        {
            zarena                    = sArenas.m_arena_free_head;       // Get the first arena from the free list
            sArenas.m_arena_free_head = sArenas.m_arena_free_head->Next; // Pop the first arena from the free list
        }
        else if (sArenas.m_arena_free_index < sArenas.m_arena_max_index)
        {
            zarena = gArenaIndexToPtr(sArenas.m_arena_free_index++); // Get the next arena from the array
        }
        else if (sArenas.m_array.CapacityCommited < sArenas.m_array.CapacityReserved)
        {
            // Commit more memory for the arenas if we have reached the maximum index
            // Increase capacity by 12.5%
            const s32   addIndices  = math::g_clamp<s32>(sArenas.m_arena_max_index >> 3, 1, sArenas.m_arena_cap_index);
            const int_t addCapacity = NumBytesToPages(sArenas.m_array, (int_t)(addIndices * sizeof(zarena_t)));
            if (!nvmem::commit(sArenas.m_array.Mem + CommittedInBytes(sArenas.m_array), NumPagesToBytes(sArenas.m_array, addCapacity)))
            {
                arena_error(cArenaErrorCommitMemory);
                return nullptr;
            }
            sArenas.m_array.CapacityCommited += addCapacity;
            sArenas.m_arena_max_index += addIndices;
        }

        zarena->Name  = "none";
        zarena->Next  = nullptr;
        zarena->Arena = arena;
        return &zarena->Arena;
    }

    void ArenaRelease(arena_t* arena)
    {
        if (arena == nullptr)
            return;

        // Release commited and reserved memory
        if (!nvmem::decommit(arena->Mem, CommittedInBytes(*arena)))
        {
            arena_error(cArenaErrorRelease);
        }
        if (!nvmem::release(arena->Mem, ReservedInBytes(*arena)))
        {
            arena_error(cArenaErrorRelease);
        }

        arena->Mem              = nullptr;
        arena->CapacityReserved = 0;
        arena->CapacityCommited = 0;
        arena->PageSizeShift    = 0;
        arena->AlignmentShift   = 0;

        // Add to free list
        zarena_t* zarena          = (zarena_t*)arena;          // Cast arena to zarena_t
        zarena->Name              = "none";                    // Reset the name to "none"
        zarena->Next              = sArenas.m_arena_free_head; // Link the arena to the head of the free list
        sArenas.m_arena_free_head = zarena;                    // Update the head of the free list
    }

    int_t ArenaPos(const arena_t* arena)
    {
        if (arena == nullptr)
            return -1; // Invalid arena pointer
        return arena->Pos;
    }

    static bool ArenaSetCapacity(arena_t* arena, int_t newCapacityInBytes)
    {
        if (arena->Mem == nullptr)
        {
            arena_error(cArenaErrorNotInitialized);
            return false; // Arena memory is not allocated
        }

        s32 const newSizeInPages = NumBytesToPages(*arena, newCapacityInBytes);
        if (newSizeInPages > arena->CapacityCommited)
        {
            if (newSizeInPages > arena->CapacityReserved)
            {
                // we cannot expand the arena beyond its reserved capacity
                arena_error(cArenaErrorGrow);
                return false;
            }

            // if we are expanding the arena, we need to commit more memory
            const int_t currentSizeInBytes = CommittedInBytes(*arena);
            const int_t newSizeInBytes     = NumPagesToBytes(*arena, newSizeInPages);

            if (!nvmem::commit(arena->Mem + currentSizeInBytes, newSizeInBytes - currentSizeInBytes))
            {
                arena_error(cArenaErrorGrow);
                return false;
            }

            arena->CapacityCommited = newSizeInPages;
        }
        else if (newSizeInPages < arena->CapacityCommited)
        {
            const int_t currentSizeInBytes = CommittedInBytes(*arena);
            const int_t newSizeInBytes     = NumPagesToBytes(*arena, newSizeInPages);
            if (!nvmem::decommit(arena->Mem + newSizeInBytes, currentSizeInBytes - newSizeInBytes))
            {
                arena_error(cArenaErrorShrink);
                return false;
            }

            arena->CapacityCommited = newSizeInPages;
        }

        return true;
    }

    void* ArenaPush(arena_t* arena, int_t size_bytes)
    {
        if (size_bytes <= 0)
        {
            arena_error(cArenaErrorGrow);
            return nullptr; // Invalid size request
        }

        if ((arena->Pos + size_bytes) > CommittedInBytes(*arena))
        {
            // Calculate the new capacity in bytes, aligned to page size
            // We add a page size to ensure we have enough space for the allocation
            const int_t newCapacity        = NumBytesToPages(*arena, arena->Pos + size_bytes);
            const int_t newCapacityInBytes = NumPagesToBytes(*arena, newCapacity);
            if (!ArenaSetCapacity(arena, newCapacityInBytes))
            {
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
        const int_t alignedPos = math::g_alignUp<int_t>(arena->Pos, alignment);
        if ((alignedPos + size_bytes) > CommittedInBytes(*arena))
        {
            if (!ArenaSetCapacity(arena, alignedPos + size_bytes))
            {
                arena_error(cArenaErrorGrow);
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
        const int_t keep_commited_pages = math::g_clamp<int_t>(NumBytesToPages(*arena, keep_commited_bytes), 0, arena->CapacityCommited);

        arena->Pos = 0;
        if (keep_commited_pages < arena->CapacityCommited)
        {
            const int_t currentSizeInBytes = CommittedInBytes(*arena);
            const int_t newSizeInBytes     = NumPagesToBytes(*arena, keep_commited_pages);
            if (newSizeInBytes < currentSizeInBytes)
            {
                if (!nvmem::decommit(arena->Mem + newSizeInBytes, currentSizeInBytes - newSizeInBytes))
                {
                    arena_error(cArenaErrorShrink);
                }
            }
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
        if (arena->Pos < 0 || arena->Pos > CommittedInBytes(*arena))
            return false;

        return true;
    }
} // namespace ncore
