#include "ccore/c_target.h"
#include "ccore/c_debug.h"
#include "ccore/c_allocator.h"

#include "cvmem/c_virtual_memory.h"

#if defined TARGET_MAC
#    include <sys/mman.h>
#    include <mach/mach_host.h>
#    include <mach/mach_port.h>
#    include <mach/mach_vm.h>
#    include <mach/vm_map.h>
#    include <mach/vm_page_size.h>
#    include <unistd.h>
#    define VMEM_PLATFORM_MAC
#endif

#if defined TARGET_PC
#    include "Windows.h"
#    define VMEM_PLATFORM_WIN32
#endif

#if !defined(TARGET_DEBUG)
#    define VMEM_NO_ERROR_CHECKING
#    define VMEM_NO_ERROR_MESSAGES
#endif

namespace ncore
{
    namespace nvmem
    {
        static const char* get_error_message(s32 error);

#if !defined(VMEM_NO_ERROR_CHECKING)
#    if !defined(VMEM_NO_ERROR_MESSAGES)
        static bool check(bool cond, s32 error)
        {
            if (cond)
            {
                const char* error_msg = get_error_message(error);
                ASSERTS(false, error_msg);
            }
            return !cond;
        }
#    else
        static bool check(bool cond, s32 error) { return true; }
#    endif

#else
        static bool check(bool cond, s32 error) { return true; }
#endif

        static const s32 ErrorNone                                    = 0;
        static const s32 ErrorAlignmentCannotBeZero                   = 1;
        static const s32 ErrorAlignmentHasToBePowerOf2                = 2;
        static const s32 ErrorCannotAllocateMemoryBlockWithSize0Bytes = 3;
        static const s32 ErrorCannotDeallocAMemoryBlockOfSize0        = 4;
        static const s32 ErrorFailedToFormatError                     = 5;
        static const s32 ErrorInvalidProtectMode                      = 6;
        static const s32 ErrorOutBufferPtrCannotBeNull                = 7;
        static const s32 ErrorOutBufferSizeCannotBe0                  = 8;
        static const s32 ErrorPtrCannotBeNull                         = 9;
        static const s32 ErrorSizeCannotBe0                           = 10;
        static const s32 ErrorVirtualAllocFailed                      = 11;
        static const s32 ErrorVirtualFreeFailed                       = 12;
        static const s32 ErrorVirtualProtectFailed                    = 13;
        static const s32 ErrorVirtualAllocReturnedNull                = 14;
        static const s32 ErrorVirtualLockFailed                       = 15;
        static const s32 ErrorVirtualUnlockFailed                     = 16;

#if !defined(VMEM_NO_ERROR_MESSAGES)

        static const char* get_error_message(s32 error)
        {
            switch (error)
            {
                case 0: return "No error";
                case ErrorAlignmentCannotBeZero: return "Alignment cannot be zero";
                case ErrorAlignmentHasToBePowerOf2: return "Alignment has to be a power of 2";
                case ErrorCannotAllocateMemoryBlockWithSize0Bytes: return "Cannot allocate memory block with size 0 bytes";
                case ErrorCannotDeallocAMemoryBlockOfSize0: return "Cannot deallocate a memory block of size 0";
                case ErrorFailedToFormatError: return "Failed to format error";
                case ErrorInvalidProtectMode: return "Invalid protect mode";
                case ErrorOutBufferPtrCannotBeNull: return "Out buffer ptr cannot be null";
                case ErrorOutBufferSizeCannotBe0: return "Out buffer size cannot be 0";
                case ErrorPtrCannotBeNull: return "Ptr cannot be null";
                case ErrorSizeCannotBe0: return "Size cannot be 0";
                case ErrorVirtualAllocFailed: return "VirtualAlloc failed";
                case ErrorVirtualFreeFailed: return "VirtualFree failed";
                case ErrorVirtualProtectFailed: return "VirtualProtect failed";
                case ErrorVirtualAllocReturnedNull: return "VirtualAlloc returned null";
                case ErrorVirtualLockFailed: return "VirtualLock failed";
                case ErrorVirtualUnlockFailed: return "VirtualUnlock failed";
            }
            return "Unknown error";
        }
#else
        static const char* get_error_message(s32 _) { return "<Error messages disabled>"; }
#endif

        // Cached global page size.
        static u32 s_page_size              = 0;
        static u32 s_allocation_granularity = 0;

        u32 get_page_size(void) { return s_page_size; }
        u32 get_allocation_granularity(void) { return s_allocation_granularity; }

        const char* get_protect_name(const nprotect::value_t protect)
        {
            switch (protect)
            {
                case nprotect::Invalid: return "INVALID";
                case nprotect::NoAccess: return "NoAccess";
                case nprotect::Read: return "Read";
                case nprotect::ReadWrite: return "ReadWrite";
                case nprotect::Execute: return "Execute";
                case nprotect::ExecuteRead: return "ExecuteRead";
                case nprotect::ExecuteReadWrite: return "ExecuteReadWrite";
            }
            return "<Unknown>";
        }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Windows backend implementation
//
#if defined(VMEM_PLATFORM_WIN32)
        static const DWORD s_protect_array[] = {0xffffffff, PAGE_NOACCESS, PAGE_READONLY, PAGE_READWRITE, PAGE_EXECUTE, PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE};
        static DWORD       _win32_protect(const nprotect::value_t protect)
        {
            DWORD const protect_win = s_protect_array[protect];
            if (protect_win == 0xffffffff)
            {
                ErrorInvalidProtectMode;
                return false;
            }
            return protect_win;
        }

        static nprotect::value_t _protect_from_win32(const DWORD protect)
        {
            switch (protect)
            {
                case PAGE_NOACCESS: return nprotect::NoAccess;
                case PAGE_READONLY: return nprotect::Read;
                case PAGE_READWRITE: return nprotect::ReadWrite;
                case PAGE_EXECUTE: return nprotect::Execute;
                case PAGE_EXECUTE_READ: return nprotect::ExecuteRead;
                case PAGE_EXECUTE_READWRITE: return nprotect::ExecuteReadWrite;
            }
            ErrorInvalidProtectMode;
            return nprotect::Invalid;
        }

        void* alloc_protect(const size_t num_bytes, const nprotect::value_t protect)
        {
            if (!check(num_bytes == 0, ErrorCannotAllocateMemoryBlockWithSize0Bytes))
                return nullptr;

            const DWORD protect_win32 = _win32_protect(protect);
            if (protect_win32)
            {
                LPVOID address = VirtualAlloc(NULL, (SIZE_T)num_bytes, MEM_RESERVE, protect_win32);
                if (!check(address == NULL, ErrorVirtualAllocReturnedNull))
                    return nullptr;
                // Note: memory is initialized to zero.
                return address;
            }
            return nullptr;
        }

        bool dealloc(void* ptr, const size_t num_allocated_bytes)
        {
            if (!check(ptr == 0, ErrorPtrCannotBeNull))
                return false;
            if (!check(num_allocated_bytes == 0, ErrorCannotDeallocAMemoryBlockOfSize0))
                return false;

            const BOOL result = VirtualFree(ptr, 0, MEM_RELEASE);
            if (!check(result == 0, ErrorVirtualFreeFailed))
                return false;
            return result ? true : false;
        }

        bool commit_protect(void* ptr, const size_t num_bytes, const nprotect::value_t protect)
        {
            if (!check(ptr == 0, ErrorPtrCannotBeNull))
                return false;
            if (!check(num_bytes == 0, ErrorSizeCannotBe0))
                return false;

            const LPVOID result = VirtualAlloc(ptr, num_bytes, MEM_COMMIT, _win32_protect(protect));
            if (!check(result == 0, ErrorVirtualAllocFailed))
                return false;
            return true;
        }

        bool decommit(void* ptr, const size_t num_bytes)
        {
            if (!check(ptr == 0, ErrorPtrCannotBeNull))
                return false;
            if (!check(num_bytes == 0, ErrorSizeCannotBe0))
                return false;

            const BOOL result = VirtualFree(ptr, num_bytes, MEM_DECOMMIT);
            if (!check(result == 0, ErrorVirtualFreeFailed))
                return false;
            return true;
        }

        bool protect(void* ptr, const size_t num_bytes, const nprotect::value_t protect)
        {
            if (!check(ptr == 0, ErrorPtrCannotBeNull))
                return false;
            if (!check(num_bytes == 0, ErrorSizeCannotBe0))
                return false;

            DWORD      old_protect = 0;
            const BOOL result      = VirtualProtect(ptr, num_bytes, _win32_protect(protect), &old_protect);
            if (!check(result == 0, ErrorVirtualProtectFailed))
                return false;
            return true;
        }

        u32 query_page_size(void)
        {
            SYSTEM_INFO system_info = {0};
            GetSystemInfo(&system_info);
            return (u32)system_info.dwPageSize;
        }

        u32 query_allocation_granularity(void)
        {
            SYSTEM_INFO system_info = {0};
            GetSystemInfo(&system_info);
            return (u32)system_info.dwAllocationGranularity;
        }

        usage_t query_usage_status(void)
        {
            MEMORYSTATUS status = {0};
            GlobalMemoryStatus(&status);

            usage_t usage_status              = {0};
            usage_status.total_physical_bytes = status.dwTotalPhys;
            usage_status.avail_physical_bytes = status.dwAvailPhys;

            return usage_status;
        }

        bool lock(void* ptr, const size_t num_bytes)
        {
            if (!check(ptr == 0, ErrorPtrCannotBeNull))
                return false;
            if (!check(num_bytes == 0, ErrorSizeCannotBe0))
                return false;

            const BOOL result = VirtualLock(ptr, num_bytes);
            if (!check(result == 0, ErrorVirtualLockFailed))
                return false;
            return true;
        }

        bool unlock(void* ptr, const size_t num_bytes)
        {
            if (!check(ptr == 0, ErrorPtrCannotBeNull))
                return false;
            if (!check(num_bytes == 0, ErrorSizeCannotBe0))
                return false;

            const BOOL result = VirtualUnlock(ptr, num_bytes);
            if (!check(result == 0, ErrorVirtualUnlockFailed))
                return false;
            return true;
        }

#endif // defined(VMEM_PLATFORM_WIN32)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MacOS backend implementation
//
#if defined(VMEM_PLATFORM_MAC)
        static const s32 s_protect_array[] = {-1, PROT_NONE, PROT_READ, PROT_READ | PROT_WRITE, PROT_EXEC, PROT_EXEC | PROT_READ, PROT_EXEC | PROT_READ | PROT_WRITE};

        static s32 _mac_protect(const nprotect::value_t protect)
        {
            s32 const protect_mac = s_protect_array[protect];
            if (protect_mac == -1)
            {
                check(false, ErrorInvalidProtectMode);
                return 0;
            }
            return protect_mac;
        }

        static nprotect::value_t _protect_from_mac(const s32 protect)
        {
            switch (protect)
            {
                case PROT_NONE: return nprotect::NoAccess;
                case PROT_READ: return nprotect::Read;
                case PROT_READ | PROT_WRITE: return nprotect::ReadWrite;
                case PROT_EXEC: return nprotect::Execute;
                case PROT_EXEC | PROT_READ: return nprotect::ExecuteRead;
                case PROT_EXEC | PROT_READ | PROT_WRITE: return nprotect::ExecuteReadWrite;
            }
            check(false, ErrorInvalidProtectMode);
            return nprotect::Invalid;
        }

        void* alloc_protect(const size_t num_bytes, const nprotect::value_t protect)
        {
            if (!check(num_bytes == 0, ErrorCannotAllocateMemoryBlockWithSize0Bytes))
                return nullptr;

            const s32 protect_mac = _mac_protect(protect);
            if (protect_mac)
            {
                void* address = mmap(nullptr, num_bytes, protect_mac, MAP_PRIVATE | MAP_ANON, -1, 0);
                if (!check(address == MAP_FAILED, ErrorFailedToFormatError))
                    return nullptr;
                return address;
            }

            return 0;
        }

        bool dealloc(void* ptr, const size_t num_allocated_bytes)
        {
            if (!check(ptr == 0, ErrorPtrCannotBeNull))
                return false;
            if (!check(num_allocated_bytes == 0, ErrorCannotDeallocAMemoryBlockOfSize0))
                return false;

            const s32 result = munmap(ptr, num_allocated_bytes);
            if (!check(result == -1, ErrorFailedToFormatError))
                return false;
            return true;
        }

        bool commit_protect(void* ptr, const size_t num_bytes, const nprotect::value_t protect)
        {
            if (!check(ptr == 0, ErrorPtrCannotBeNull))
                return false;
            if (!check(num_bytes == 0, ErrorSizeCannotBe0))
                return false;

            const s32 protect_mac = _mac_protect(protect);
            if (protect_mac)
            {
                const s32 result = mprotect(ptr, num_bytes, protect_mac);
                if (!check(result == -1, ErrorFailedToFormatError))
                    return false;
                return true;
            }

            return false;
        }

        bool decommit(void* ptr, const size_t num_bytes)
        {
            if (!check(ptr == 0, ErrorPtrCannotBeNull))
                return false;
            if (!check(num_bytes == 0, ErrorSizeCannotBe0))
                return false;

            const s32 result = mprotect(ptr, num_bytes, PROT_NONE);
            if (!check(result == -1, ErrorFailedToFormatError))
                return false;
            return true;
        }

        bool protect(void* ptr, const size_t num_bytes, const nprotect::value_t protect)
        {
            if (!check(ptr == 0, ErrorPtrCannotBeNull))
                return false;
            if (!check(num_bytes == 0, ErrorSizeCannotBe0))
                return false;

            const s32 protect_mac = _mac_protect(protect);
            if (protect_mac)
            {
                const s32 result = mprotect(ptr, num_bytes, protect_mac);
                if (!check(result == -1, ErrorFailedToFormatError))
                    return false;
                return true;
            }

            return false;
        }

        u32 query_page_size(void) { return (size_t)vm_page_size; }
        u32 query_allocation_granularity(void) { return (size_t)vm_page_size; }

        usage_t query_usage_status(void)
        {
            usage_t usage_status              = {0};
            usage_status.total_physical_bytes = 0;
            usage_status.avail_physical_bytes = 0;

            mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
            vm_statistics64_data_t vm_stat;
            if (host_statistics64(mach_host_self(), HOST_VM_INFO, (host_info_t)&vm_stat, &count) == KERN_SUCCESS)
            {
                usage_status.total_physical_bytes = (size_t)vm_stat.wire_count + (size_t)vm_stat.active_count + (size_t)vm_stat.inactive_count + (size_t)vm_stat.free_count;
                usage_status.avail_physical_bytes = (size_t)vm_stat.free_count;
            }

            return usage_status;
        }

        bool lock(void* ptr, const size_t num_bytes)
        {
            if (!check(ptr == 0, ErrorPtrCannotBeNull))
                return false;
            if (!check(num_bytes == 0, ErrorSizeCannotBe0))
                return false;

            const s32 result = mlock(ptr, num_bytes);
            if (!check(result == -1, ErrorVirtualLockFailed))
                return false;
            return true;
        }

        bool unlock(void* ptr, const size_t num_bytes)
        {
            if (!check(ptr == 0, ErrorPtrCannotBeNull))
                return false;
            if (!check(num_bytes == 0, ErrorSizeCannotBe0))
                return false;

            const s32 result = munlock(ptr, num_bytes);
            if (!check(result == -1, ErrorVirtualUnlockFailed))
                return false;
            return true;
        }

#endif

        bool       reserve(u64 address_range, nprotect::value_t attributes, void*& baseptr)
        {
            baseptr = alloc_protect(address_range, attributes);
            return baseptr != nullptr;
        }

        u32 page_size() { return s_page_size; }

        bool release(void* baseptr, u64 address_range) { return dealloc(baseptr, address_range) == true; }
        bool commit(void* page_address, u64 size) { return commit_protect(page_address, size, nprotect::ReadWrite) == true; }

        bool initialize()
        {
            // Note: this will be 2 syscalls on windows.
            s_page_size              = query_page_size();
            s_allocation_granularity = query_allocation_granularity();
            return true;
        }
    } // namespace nvmem
}; // namespace ncore
