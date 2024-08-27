#ifndef __C_VMEM_VIRTUAL_MEMORY_INTERFACE_H__
#define __C_VMEM_VIRTUAL_MEMORY_INTERFACE_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

namespace ncore
{
    struct vmem_protect_t
    {
        u8 value;

        static const u8 Invalid;
        static const u8 NoAccess;         // The page memory cannot be accessed at all.
        static const u8 Read;             // You can only read from the page memory .
        static const u8 ReadWrite;        // You can read and write to the page memory. This is the most common option.
        static const u8 Execute;          // You can only execute the page memory .
        static const u8 ExecuteRead;      // You can execute the page memory and read from it.
        static const u8 ExecuteReadWrite; // You can execute the page memory and read/write to it.
        static const u8 COUNT;
    };

    class vmem_t
    {
    public:
        static bool initialize();

        static u32 page_size();

        static bool reserve(u64 address_range, vmem_protect_t attributes, void*& baseptr);
        static bool release(void* baseptr, u64 address_range);

        static bool commit(void* address, u64 size);
        static bool decommit(void* address, u64 size);
    };

    typedef int_t vmem_size_t;

    // Success/Error result (vmem_result_t).
    // You can use this as a bool in an if statement: if(vmem_commit(...)) { do something... }
    struct vmem_result_t
    {
        static const s8 Error   = 0; // false
        static const s8 Success = 1; // true

        inline bool IsError() const { return value == Error; }
        inline bool IsSuccess() const { return value == Success; }

        inline operator bool() const { return value == Success; }
        s8     value;
    };

    // Global memory status.
    struct vmem_usage_t
    {
        vmem_size_t total_physical_bytes;
        vmem_size_t avail_physical_bytes;
    };

    // Call once at the start of your program.
    // This exists only to cache result of `vmem_query_page_size` so you can use faster `vmem_get_page_size`,
    // so this is completely optional. If you don't call this `vmem_get_page_size` will return 0.
    // Currently there isn't any deinit/shutdown code.
    u32 vmem_init(void);

    // Reserve (allocate but don't commit) a block of static address-space of size `num_byt;
    // @returns 0 on error, start address of the allocated memory block on success.
    void* vmem_alloc_protect(vmem_size_t num_bytes, vmem_protect_t protect);

    // Reserves (allocates but doesn't commit) a block of static address-space of size `num_bytes`, in ReadWrite protec;
    // mode. The memory is zeroed. Dealloc with `vmem_dealloc`. Note: you must commit the memory before using it.
    // To maximize efficiency, try to always use a multiple of allocation granularity (see
    // `vmem_get_allocation_granularity`) for size of allocations.
    // @param num_bytes: total size of the memory block.
    // @returns 0 on error, start address of the allocated memory block on success.
    inline void* vmem_alloc(const vmem_size_t num_bytes) { return vmem_alloc_protect(num_bytes, {vmem_protect_t::ReadWrite}); }

    // Commit memory pages which contain one or more bytes in [ptr...ptr+num_bytes]. The pages will be mapped to physical
    // memory.
    // Decommit with `vmem_decommit`.
    // @param ptr: pointer to the pointer returned by `vmem_alloc` or shifted by [0...num_bytes].
    vmem_result_t vmem_commit_protect(void* ptr, vmem_size_t num_bytes, vmem_protect_t protect);

    // Allocates memory and commits all of it.
    inline void* vmem_alloc_commited(const vmem_size_t num_bytes)
    {
        void* ptr = vmem_alloc(num_bytes);
        vmem_commit_protect(ptr, num_bytes, {vmem_protect_t::ReadWrite});
        return ptr;
    }

    // Commit memory pages which contain one or more bytes in [ptr...ptr+num_bytes]. The pages will be mapped to physical
    // memory. The page protection mode will be changed to ReadWrite. Use `vmem_commit_protect` to specify a different mode.
    // Decommit with `vmem_decommit`.
    // @param ptr: pointer to the pointer returned by `vmem_alloc` or shifted by N.
    // @param num_bytes: number of bytes to commit.
    inline vmem_result_t vmem_commit(void* ptr, const vmem_size_t num_bytes) { return vmem_commit_protect(ptr, num_bytes, {vmem_protect_t::ReadWrite}); }

    // Commit a specific number of bytes from the region. This can be used for a custom arena allocator.
    // If `commited < prev_commited`, this will shrink the usable range.
    // If `commited > prev_commited`, this will expand the usable range.
    vmem_result_t vmem_partially_commit_region(void* ptr, vmem_size_t num_bytes, vmem_size_t prev_commited, vmem_size_t commited);

    // Dealloc (release, free) a block of static mem;
    // @param alloc_ptr: a pointer to the start of the memory block. Must be the result of `vmem_alloc`.
    // @param num_allocated_bytes: *must* be the value returned by `vmem_alloc`.
    //  It isn't used on windows, but it's required on unix platforms.
    vmem_result_t vmem_dealloc(void* alloc_ptr, vmem_size_t num_allocated_bytes);

    // Decommits the memory pages which contain one or more bytes in [ptr...ptr+num_bytes]. The pages will be unmapped from
    // physical memory.
    // @param ptr: pointer to the pointer returned by `vmem_alloc` or shifted by [0...num_bytes].
    // @param num_bytes: number of bytes to decommit.
    vmem_result_t vmem_decommit(void* ptr, vmem_size_t num_bytes);

    // Sets protection mode for the region of pages. All of the pages must be commited.
    vmem_result_t vmem_protect(void* ptr, vmem_size_t num_bytes, vmem_protect_t protect);

    // @returns cached value from `vmem_query_page_size`. Returns 0 if you don't call `vmem_init`.
    u32 vmem_get_page_size(void);

    // Query the page size from the system. Usually something like 4096 bytes.
    // @returns the page size in number bytes. Cannot fail.
    u32 vmem_query_page_size(void);

    // @returns cached value from `vmem_query_allocation_granularity`. Returns 0 if you don't call `vmem_init`.
    u32 vmem_get_allocation_granularity(void);

    // Query the allocation granularity (alignment of each allocation) from the system.
    // Usually 65KB on Windows and 4KB on linux (on linux it's page size).
    // @returns allocation granularity in bytes.
    u32 vmem_query_allocation_granularity(void);

    // Query the memory usage status from the system.
    vmem_usage_t vmem_query_usage_status(void);

    // Locks the specified region of the process's static address space into physical memory, ensuring that subseq;
    // access to the region will not incur a page fault.
    // All pages in the specified region must be commited.
    // You cannot lock pages with `VMemProtect_NoAccess`.
    vmem_result_t vmem_lock(void* ptr, vmem_size_t num_bytes);

    // Unlocks a specified range of pages in the static address space of a process, enabling the system to swap the p;
    // out to the paging file if necessary.
    // If you try to unlock pages which aren't locked, this will fail.
    vmem_result_t vmem_unlock(void* ptr, vmem_size_t num_bytes);

    // Returns a static string for the protection mode.
    // e.g. vmem_protect_t::ReadWrite will return "ReadWrite".
    // Never fails - unknown values return "<Unknown>", never null pointer.
    const char* vmem_get_protect_name(vmem_protect_t protect);

    // Round the `address` up to the next (or current) aligned address.
    // @param align: Address alignment. Must be a power of 2 and greater than 0.
    // @returns aligned address on success, VMemResult_Error on error.
    ptr_t vmem_align_forward(const ptr_t address, const u32 align);

    // Round the `address` down to the previous (or current) aligned address.
    // @param align: Address alignment. Must be a power of 2 and greater than 0.
    // @returns aligned address on success, VMemResult_Error on error.
    ptr_t vmem_align_backward(const ptr_t address, const u32 align);

    // Check if an address is a multiple of `align`.
    vmem_result_t vmem_is_aligned(const ptr_t address, const u32 align);

    // Faster version of `vmem_align_forward`, because it doesn't do any error checking and can be inlined.
    inline ptr_t vmem_align_forward_fast(const ptr_t address, const u32 align) { return (address + (ptr_t)(align - 1)) & ~(ptr_t)(align - 1); }

    // Faster version of `vmem_align_backward`, because it doesn't do any error checking and can be inlined.
    inline ptr_t vmem_align_backward_fast(const ptr_t address, const u32 align) { return address & ~(ptr_t)(align - 1); }

    // Faster version of `vmem_is_aligned`, because it doesn't do any error checking and can be inlined.
    // The alignment must be a power of 2.
    inline vmem_result_t vmem_is_aligned_fast(const ptr_t address, const u32 align)
    {
        s8 result = (address & (ptr_t)(align - 1)) == 0 ? vmem_result_t::Success : vmem_result_t::Error;
        return {result};
    }

}; // namespace ncore

#endif /// __C_VMEM_VIRTUAL_MEMORY_INTERFACE_H__
