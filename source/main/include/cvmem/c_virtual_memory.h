#ifndef __C_VMEM_VIRTUAL_MEMORY_INTERFACE_H__
#define __C_VMEM_VIRTUAL_MEMORY_INTERFACE_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

namespace ncore
{
    class vmem_t
    {
    public:
        virtual bool initialize(u32 pagesize = 0) = 0;

        virtual bool reserve(u64 address_range, u32& page_size, u32 attributes, void*& baseptr) = 0;
        virtual bool release(void* baseptr, u64 address_range)                                  = 0;

        virtual bool commit(void* address, u32 page_size, u32 page_count)   = 0;
        virtual bool decommit(void* address, u32 page_size, u32 page_count) = 0;
    };

    extern vmem_t* vmem;

#define VMEM_INLINE inline
    typedef int_t VMemSize;

    // Success/Error result (VMemResult_).
    // You can use this as a bool in an if statement: if(vmem_commit(...)) { do something... }
    typedef s32 VMemResult;
    namespace nVMemResult
    {
        const VMemResult Error   = 0; // false
        const VMemResult Success = 1; // true
    }                                 // namespace nVMemResult

    // This will return the last message after a function fails (VMemResult_Error).
    const char* vmem_get_error_message(void);

    typedef u8 VMemProtect;
    namespace nVMemProtect
    {
        const VMemProtect Invalid          = 0;
        const VMemProtect NoAccess         = 1; // The page memory cannot be accessed at all.
        const VMemProtect Read             = 2; // You can only read from the page memory .
        const VMemProtect ReadWrite        = 3; // You can read and write to the page memory. This is the most common option.
        const VMemProtect Execute          = 4; // You can only execute the page memory .
        const VMemProtect ExecuteRead      = 5; // You can execute the page memory and read from it.
        const VMemProtect ExecuteReadWrite = 6; // You can execute the page memory and read/write to it.
        const VMemProtect COUNT            = 7;
    } // namespace nVMemProtect

    // Global memory status.
    struct VMemUsageStatus
    {
        VMemSize total_physical_bytes;
        VMemSize avail_physical_bytes;
    };

    // Call once at the start of your program.
    // This exists only to cache result of `vmem_query_page_size` so you can use faster `vmem_get_page_size`,
    // so this is completely optional. If you don't call this `vmem_get_page_size` will return 0.
    // Currently there isn't any deinit/shutdown code.
    void vmem_init(void);

    // Reserve (allocate but don't commit) a block of virtual address-space of size `num_bytes`.
    // @returns 0 on error, start address of the allocated memory block on success.
    void* vmem_alloc_protect(VMemSize num_bytes, VMemProtect protect);

    // Reserves (allocates but doesn't commit) a block of virtual address-space of size `num_bytes`, in ReadWrite protection
    // mode. The memory is zeroed. Dealloc with `vmem_dealloc`. Note: you must commit the memory before using it.
    // To maximize efficiency, try to always use a multiple of allocation granularity (see
    // `vmem_get_allocation_granularity`) for size of allocations.
    // @param num_bytes: total size of the memory block.
    // @returns 0 on error, start address of the allocated memory block on success.
    inline void* vmem_alloc(const VMemSize num_bytes) { return vmem_alloc_protect(num_bytes, nVMemProtect::ReadWrite); }

    // Allocates memory and commits all of it.
    inline void* vmem_alloc_commited(const VMemSize num_bytes)
    {
        void* ptr = vmem_alloc(num_bytes);
        vmem_commit_protect(ptr, num_bytes, nVMemProtect::ReadWrite);
        return ptr;
    }

    // Commit memory pages which contain one or more bytes in [ptr...ptr+num_bytes]. The pages will be mapped to physical
    // memory. The page protection mode will be changed to ReadWrite. Use `vmem_commit_protect` to specify a different mode.
    // Decommit with `vmem_decommit`.
    // @param ptr: pointer to the pointer returned by `vmem_alloc` or shifted by N.
    // @param num_bytes: number of bytes to commit.
    inline VMemResult vmem_commit(void* ptr, const VMemSize num_bytes) { return vmem_commit_protect(ptr, num_bytes, nVMemProtect::ReadWrite); }

    // Commit a specific number of bytes from the region. This can be used for a custom arena allocator.
    // If `commited < prev_commited`, this will shrink the usable range.
    // If `commited > prev_commited`, this will expand the usable range.
    VMemResult vmem_partially_commit_region(void* ptr, VMemSize num_bytes, VMemSize prev_commited, VMemSize commited);

    // Dealloc (release, free) a block of virtual memory.
    // @param alloc_ptr: a pointer to the start of the memory block. Must be the result of `vmem_alloc`.
    // @param num_allocated_bytes: *must* be the value returned by `vmem_alloc`.
    //  It isn't used on windows, but it's required on unix platforms.
    VMemResult vmem_dealloc(void* alloc_ptr, VMemSize num_allocated_bytes);

    // Commit memory pages which contain one or more bytes in [ptr...ptr+num_bytes]. The pages will be mapped to physical
    // memory.
    // Decommit with `vmem_decommit`.
    // @param ptr: pointer to the pointer returned by `vmem_alloc` or shifted by [0...num_bytes].
    VMemResult vmem_commit_protect(void* ptr, VMemSize num_bytes, VMemProtect protect);

    // Decommits the memory pages which contain one or more bytes in [ptr...ptr+num_bytes]. The pages will be unmapped from
    // physical memory.
    // @param ptr: pointer to the pointer returned by `vmem_alloc` or shifted by [0...num_bytes].
    // @param num_bytes: number of bytes to decommit.
    VMemResult vmem_decommit(void* ptr, VMemSize num_bytes);

    // Sets protection mode for the region of pages. All of the pages must be commited.
    VMemResult vmem_protect(void* ptr, VMemSize num_bytes, VMemProtect protect);

    // @returns cached value from `vmem_query_page_size`. Returns 0 if you don't call `vmem_init`.
    VMemSize vmem_get_page_size(void);

    // Query the page size from the system. Usually something like 4096 bytes.
    // @returns the page size in number bytes. Cannot fail.
    VMemSize vmem_query_page_size(void);

    // @returns cached value from `vmem_query_allocation_granularity`. Returns 0 if you don't call `vmem_init`.
    VMemSize vmem_get_allocation_granularity(void);

    // Query the allocation granularity (alignment of each allocation) from the system.
    // Usually 65KB on Windows and 4KB on linux (on linux it's page size).
    // @returns allocation granularity in bytes.
    VMemSize vmem_query_allocation_granularity(void);

    // Query the memory usage status from the system.
    VMemUsageStatus vmem_query_usage_status(void);

    // Locks the specified region of the process's virtual address space into physical memory, ensuring that subsequent
    // access to the region will not incur a page fault.
    // All pages in the specified region must be commited.
    // You cannot lock pages with `VMemProtect_NoAccess`.
    VMemResult vmem_lock(void* ptr, VMemSize num_bytes);

    // Unlocks a specified range of pages in the virtual address space of a process, enabling the system to swap the pages
    // out to the paging file if necessary.
    // If you try to unlock pages which aren't locked, this will fail.
    VMemResult vmem_unlock(void* ptr, VMemSize num_bytes);

    // Returns a static string for the protection mode.
    // e.g. nVMemProtect::ReadWrite will return "ReadWrite".
    // Never fails - unknown values return "<Unknown>", never null pointer.
    const char* vmem_get_protect_name(VMemProtect protect);

    // Round the `address` up to the next (or current) aligned address.
    // @param align: Address alignment. Must be a power of 2 and greater than 0.
    // @returns aligned address on success, VMemResult_Error on error.
    ptr_t vmem_align_forward(const ptr_t address, const s32 align);

    // Round the `address` down to the previous (or current) aligned address.
    // @param align: Address alignment. Must be a power of 2 and greater than 0.
    // @returns aligned address on success, VMemResult_Error on error.
    ptr_t vmem_align_backward(const ptr_t address, const s32 align);

    // Check if an address is a multiple of `align`.
    VMemResult vmem_is_aligned(const ptr_t address, const s32 align);

    // Faster version of `vmem_align_forward`, because it doesn't do any error checking and can be inlined.
    inline ptr_t vmem_align_forward_fast(const ptr_t address, const s32 align) { return (address + (ptr_t)(align - 1)) & ~(ptr_t)(align - 1); }

    // Faster version of `vmem_align_backward`, because it doesn't do any error checking and can be inlined.
    inline ptr_t vmem_align_backward_fast(const ptr_t address, const s32 align) { return address & ~(align - 1); }

    // Faster version of `vmem_is_aligned`, because it doesn't do any error checking and can be inlined.
    // The alignment must be a power of 2.
    inline VMemResult vmem_is_aligned_fast(const ptr_t address, const s32 align) { return (address & (align - 1)) == 0; }

}; // namespace ncore

#endif /// __C_VMEM_VIRTUAL_MEMORY_INTERFACE_H__