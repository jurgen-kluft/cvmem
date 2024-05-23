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
#if !defined(VMEM_NO_ERROR_CHECKING)
#    if !defined(VMEM_NO_ERROR_MESSAGES)
    static void vmem__write_error_message(const char* error_msg) { ASSERTS(false, error_msg); }
#    else
    static void vmem__write_error_message(const char* error_msg) {}
#    endif

    static bool vmem_check(bool cond, const char* error_msg)
    {
        vmem__write_error_message(error_msg);
        return cond;
    }

#else
    static bool vmem_check(bool cond, const char* error_msg) { return true; }
#endif

    vmem_arena_t vmem_arena_init(void* mem, const vmem_size_t size_bytes)
    {
        if (!vmem_is_aligned((ptr_t)mem, vmem_get_page_size()))
        {
            vmem__write_error_message("Arena must be aligned to page size.");
            return (vmem_arena_t){0};
        }
        if (size_bytes == 0)
        {
            vmem__write_error_message("Size cannot be 0.");
            return (vmem_arena_t){0};
        }
        vmem_arena_t arena = {0};
        arena.mem          = (u8*)mem;
        arena.size_bytes   = size_bytes;
        return arena;
    }

    vmem_arena_t vmem_arena_init_alloc(vmem_size_t size_bytes)
    {
        if (size_bytes == 0)
        {
            vmem__write_error_message("Arena size cannot be zero.");
            return (vmem_arena_t){0};
        }
        vmem_arena_t arena = {0};
        arena.mem          = (u8*)vmem_alloc(size_bytes);
        arena.size_bytes   = size_bytes;
        return arena;
    }

    vmem_result_t vmem_arena_deinit_dealloc(vmem_arena_t* arena)
    {
        vmem_check(arena == 0, "Arena pointer is null.");
        const vmem_result_t result = vmem_dealloc(arena->mem, arena->size_bytes);
        arena->mem                 = 0;
        return result;
    }

    vmem_result_t vmem_partially_commit_region(void* ptr, vmem_size_t num_bytes, vmem_size_t prev_commited, vmem_size_t commited)
    {
        if (commited == prev_commited)
            return {vmem_result_t::Success};

        // If you hit this, you likely either didn't alloc enough space up-front,
        // or have a leak that is allocating too many elements
        vmem_check(commited > num_bytes, "Cannot commit more memory than is available.");

        const vmem_size_t new_commited_bytes     = vmem_arena_calc_bytes_used_for_size(commited);
        const vmem_size_t current_commited_bytes = vmem_arena_calc_bytes_used_for_size(prev_commited);

        if (new_commited_bytes == current_commited_bytes)
            return {vmem_result_t::Success};

        // Shrink
        if (new_commited_bytes < current_commited_bytes)
        {
            const vmem_size_t bytes_to_decommit = (vmem_size_t)((ptr_t)current_commited_bytes - (ptr_t)new_commited_bytes);
            return vmem_decommit((void*)((ptr_t)ptr + new_commited_bytes), bytes_to_decommit);
        }
        // Expand
        if (new_commited_bytes > current_commited_bytes)
        {
            return vmem_commit(ptr, new_commited_bytes);
        }

        return {vmem_result_t::Success};
    }

    vmem_result_t vmem_arena_set_commited(vmem_arena_t* arena, const vmem_size_t commited)
    {
        vmem_check(arena == 0, "Arena pointer is null.");
        vmem_result_t result = vmem_partially_commit_region(arena->mem, arena->size_bytes, arena->commited, commited);
        if (result.IsError())
            return {vmem_result_t::Error};

        arena->commited = commited;
        return {vmem_result_t::Success};
    }

} // namespace ncore
