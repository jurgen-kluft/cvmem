#include "ccore/c_target.h"
#include "ccore/c_debug.h"

#include "cvmem/c_virtual_memory.h"
#include "cvmem/c_virtual_arena.h"

#if !defined(TARGET_DEBUG)
#    define VMEM_NO_ERROR_CHECKING
#    define VMEM_NO_ERROR_MESSAGES
#endif

namespace ncore
{
    namespace nvmem
    {
#if !defined(VMEM_NO_ERROR_CHECKING)
#    if !defined(VMEM_NO_ERROR_MESSAGES)
        static void _write_error_message(const char* error_msg) { ASSERTS(false, error_msg); }
#    else
        static void _write_error_message(const char* error_msg) {}
#    endif

        static bool check(bool cond, const char* error_msg)
        {
            _write_error_message(error_msg);
            return cond;
        }

#else
        static void _write_error_message(const char* error_msg) {}
        static bool check(bool cond, const char* error_msg) { return true; }
#endif

        arena_t arena_init(void* mem, const size_t size_bytes)
        {
            arena_t arena = {0};
            if (!is_aligned((ptr_t)mem, get_page_size()))
            {
                _write_error_message("Arena must be aligned to page size.");
                return arena;
            }
            if (size_bytes == 0)
            {
                _write_error_message("Size cannot be 0.");
                return arena;
            }
            arena.mem        = (u8*)mem;
            arena.size_bytes = size_bytes;
            return arena;
        }

        arena_t arena_init_alloc(size_t size_bytes)
        {
            arena_t arena = {0};
            if (size_bytes == 0)
            {
                _write_error_message("Arena size cannot be zero.");
                return arena;
            }
            arena.mem        = (u8*)alloc(size_bytes);
            arena.size_bytes = size_bytes;
            return arena;
        }

        bool arena_deinit_dealloc(arena_t* arena)
        {
            check(arena == 0, "Arena pointer is null.");
            const bool result = dealloc(arena->mem, arena->size_bytes);
            arena->mem                 = 0;
            return result;
        }

        bool partially_commit_region(void* ptr, size_t num_bytes, size_t prev_commited, size_t commited)
        {
            if (commited == prev_commited)
                return true;

            // If you hit this, you likely either didn't alloc enough space up-front,
            // or have a leak that is allocating too many elements
            check(commited > num_bytes, "Cannot commit more memory than is available.");

            const size_t new_commited_bytes     = arena_calc_bytes_used_for_size(commited);
            const size_t current_commited_bytes = arena_calc_bytes_used_for_size(prev_commited);

            if (new_commited_bytes == current_commited_bytes)
                return true;

            // Shrink
            if (new_commited_bytes < current_commited_bytes)
            {
                const size_t bytes_to_decommit = (size_t)((ptr_t)current_commited_bytes - (ptr_t)new_commited_bytes);
                return decommit((void*)((ptr_t)ptr + new_commited_bytes), bytes_to_decommit);
            }
            // Expand
            if (new_commited_bytes > current_commited_bytes)
            {
                return commit(ptr, new_commited_bytes);
            }

            return true;
        }

        bool arena_set_commited(arena_t* arena, const size_t commited)
        {
            check(arena == 0, "Arena pointer is null.");
            bool result = partially_commit_region(arena->mem, arena->size_bytes, arena->commited, commited);
            if (!result)
                return false;

            arena->commited = commited;
            return true;
        }
    } // namespace nvmem
} // namespace ncore
