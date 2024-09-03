#ifndef __C_VIRTUAL_MEMORY_INTERFACE_H__
#define __C_VIRTUAL_MEMORY_INTERFACE_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

namespace ncore
{
    namespace nvmem
    {
        typedef u64 size_t;
        typedef s8 protect_t;

        const protect_t Invalid = 0;
        const protect_t NoAccess = 1;         // The page memory cannot be accessed at all.
        const protect_t Read = 2;             // You can only read from the page memory .
        const protect_t ReadWrite = 3;        // You can read and write to the page memory. This is the most common option.
        const protect_t Execute = 4;          // You can only execute the page memory .
        const protect_t ExecuteRead = 5;      // You can execute the page memory and read from it.
        const protect_t ExecuteReadWrite = 6; // You can execute the page memory and read/write to it.

        // Call once at the start of your program.
        // This exists only to cache result of `query_page_size` so you can use faster `get_page_size`,
        // so this is completely optional. If you don't call this `get_page_size` will return 0.
        // Currently there isn't any deinit/shutdown code.
        bool initialize();

        u32 page_size();

        bool reserve(u64 address_range, protect_t attributes, void*& baseptr);
        bool release(void* baseptr, u64 address_range);

        bool commit(void* address, u64 size);
        bool decommit(void* address, u64 size);

        // Global memory status.
        struct usage_t
        {
            size_t total_physical_bytes;
            size_t avail_physical_bytes;
        };

        // Reserves (allocates but doesn't commit) a block of static address-space of size `num_bytes`, in ReadWrite protec;
        // mode. The memory is zeroed. Dealloc with `dealloc`. Note: you must commit the memory before using it.
        // To maximize efficiency, try to always use a multiple of allocation granularity (see
        // `get_allocation_granularity`) for size of allocations.
        // @param num_bytes: total size of the memory block.
        // @returns 0 on error, start address of the allocated memory block on success.
        void* alloc(size_t num_bytes);

        // Allocates memory and commits all of it.
        void* alloc_and_commit(const size_t num_bytes);

        // Reserve (allocate but don't commit) a block of static address-space of size `num_bytes`
        // @returns 0 on error, start address of the allocated memory block on success.
        void* alloc_protect(size_t num_bytes, protect_t protect);

        // Dealloc (release, free) a block of static mem;
        // @param alloc_ptr: a pointer to the start of the memory block. Must be the result of `alloc`.
        // @param num_allocated_bytes: *must* be the value returned by `alloc`.
        //  It isn't used on windows, but it's required on unix platforms.
        bool dealloc(void* alloc_ptr, size_t num_allocated_bytes);

        // Commit memory pages which contain one or more bytes in [ptr...ptr+num_bytes]. The pages will be mapped to physical
        // memory.
        // Decommit with `decommit`.
        // @param ptr: pointer to the pointer returned by `alloc` or shifted by [0...num_bytes].
        bool commit_protect(void* ptr, size_t num_bytes, protect_t protect);

        // Commit memory pages which contain one or more bytes in [ptr...ptr+num_bytes]. The pages will be mapped to physical
        // memory. The page protection mode will be changed to ReadWrite. Use `commit_protect` to specify a different mode.
        // Decommit with `decommit`.
        // @param ptr: pointer to the pointer returned by `alloc` or shifted by N.
        // @param num_bytes: number of bytes to commit.
        bool commit(void* ptr, const size_t num_bytes);

        // Decommits the memory pages which contain one or more bytes in [ptr...ptr+num_bytes]. The pages will be unmapped from
        // physical memory.
        // @param ptr: pointer to the pointer returned by `alloc` or shifted by [0...num_bytes].
        // @param num_bytes: number of bytes to decommit.
        bool decommit(void* ptr, size_t num_bytes);

        // Commit a specific number of bytes from the region. This can be used for a custom arena allocator.
        // If `commited < prev_commited`, this will shrink the usable range.
        // If `commited > prev_commited`, this will expand the usable range.
        bool partially_commit_region(void* ptr, size_t num_bytes, size_t prev_commited, size_t commited);

        // Sets protection mode for the region of pages. All of the pages must be commited.
        bool protect(void* ptr, size_t num_bytes, protect_t protect);

        // @returns cached value from `query_page_size`. Returns 0 if you don't call `init`.
        u32 get_page_size(void);

        // Query the page size from the system. Usually something like 4096 bytes.
        // @returns the page size in number bytes. Cannot fail.
        u32 query_page_size(void);

        // @returns cached value from `query_allocation_granularity`. Returns 0 if you don't call `init`.
        u32 get_allocation_granularity(void);

        // Query the allocation granularity (alignment of each allocation) from the system.
        // Usually 65KB on Windows and 4KB on linux (on linux it's page size).
        // @returns allocation granularity in bytes.
        u32 query_allocation_granularity(void);

        // Query the memory usage status from the system.
        usage_t query_usage_status(void);

        // Locks the specified region of the process's static address space into physical memory, ensuring that subseq;
        // access to the region will not incur a page fault.
        // All pages in the specified region must be commited.
        // You cannot lock pages with `VMemProtect_NoAccess`.
        bool lock(void* ptr, size_t num_bytes);

        // Unlocks a specified range of pages in the static address space of a process, enabling the system to swap the p;
        // out to the paging file if necessary.
        // If you try to unlock pages which aren't locked, this will fail.
        bool unlock(void* ptr, size_t num_bytes);

        // Returns a static string for the protection mode.
        // e.g. protect_t::ReadWrite will return "ReadWrite".
        // Never fails - unknown values return "<Unknown>", never null pointer.
        const char* get_protect_name(protect_t protect);

        // Pointer arithmetic.
        inline ptr_t align_forward(const ptr_t address, const u32 align) { return (address + (ptr_t)(align - 1)) & ~(ptr_t)(align - 1); }
        inline ptr_t align_backward(const ptr_t address, const u32 align) { return address & ~(ptr_t)(align - 1); }
        inline bool  is_aligned(const ptr_t address, const u32 align) { return (address & (ptr_t)(align - 1)) == 0 ? true : false; }

        inline void* alloc(size_t num_bytes) { return alloc_protect(num_bytes, ReadWrite); }
        inline void* alloc_and_commit(const size_t num_bytes)
        {
            void* ptr = alloc(num_bytes);
            commit_protect(ptr, num_bytes, ReadWrite);
            return ptr;
        }

    }; // namespace nvmem

}; // namespace ncore

#endif /// __C_VIRTUAL_MEMORY_INTERFACE_H__
