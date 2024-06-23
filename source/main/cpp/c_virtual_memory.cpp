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
    const u8 vmem_protect_t::Invalid          = 0;
    const u8 vmem_protect_t::NoAccess         = 1; // The page memory cannot be accessed at all.
    const u8 vmem_protect_t::Read             = 2; // You can only read from the page memory .
    const u8 vmem_protect_t::ReadWrite        = 3; // You can read and write to the page memory. This is the most common option.
    const u8 vmem_protect_t::Execute          = 4; // You can only execute the page memory .
    const u8 vmem_protect_t::ExecuteRead      = 5; // You can execute the page memory and read from it.
    const u8 vmem_protect_t::ExecuteReadWrite = 6; // You can execute the page memory and read/write to it.
    const u8 vmem_protect_t::COUNT            = 7;

    static const char* vmem_get_error_message(s32 vmem_error);

#if !defined(VMEM_NO_ERROR_CHECKING)
#    if !defined(VMEM_NO_ERROR_MESSAGES)
    static bool vmem_check(bool cond, s32 error)
    {
        if (cond)
        {
            const char* error_msg = vmem_get_error_message(error);
            ASSERTS(false, error_msg);
        }
        return !cond;
    }
#    else
    static bool vmem_check(bool cond, s32 error) { return true; }
#    endif

#else
    static bool vmem_check(bool cond, s32 error) { return true; }
#endif

    static const s32 NoError                                 = 0;
    static const s32 AlignmentCannotBeZero                   = 1;
    static const s32 AlignmentHasToBePowerOf2                = 2;
    static const s32 CannotAllocateMemoryBlockWithSize0Bytes = 3;
    static const s32 CannotDeallocAMemoryBlockOfSize0        = 4;
    static const s32 FailedToFormatError                     = 5;
    static const s32 InvalidProtectMode                      = 6;
    static const s32 OutBufferPtrCannotBeNull                = 7;
    static const s32 OutBufferSizeCannotBe0                  = 8;
    static const s32 PtrCannotBeNull                         = 9;
    static const s32 SizeCannotBe0                           = 10;
    static const s32 VirtualAllocFailed                      = 11;
    static const s32 VirtualFreeFailed                       = 12;
    static const s32 VirtualProtectFailed                    = 13;
    static const s32 VirtualAllocReturnedNull                = 14;
    static const s32 VirtualLockFailed                       = 15;
    static const s32 VirtualUnlockFailed                     = 16;

#if !defined(VMEM_NO_ERROR_MESSAGES)

    static const char* vmem_get_error_message(s32 vmem_error)
    {
        switch (vmem_error)
        {
            case 0: return "No error";
            case AlignmentCannotBeZero: return "Alignment cannot be zero";
            case AlignmentHasToBePowerOf2: return "Alignment has to be a power of 2";
            case CannotAllocateMemoryBlockWithSize0Bytes: return "Cannot allocate memory block with size 0 bytes";
            case CannotDeallocAMemoryBlockOfSize0: return "Cannot deallocate a memory block of size 0";
            case FailedToFormatError: return "Failed to format error";
            case InvalidProtectMode: return "Invalid protect mode";
            case OutBufferPtrCannotBeNull: return "Out buffer ptr cannot be null";
            case OutBufferSizeCannotBe0: return "Out buffer size cannot be 0";
            case PtrCannotBeNull: return "Ptr cannot be null";
            case SizeCannotBe0: return "Size cannot be 0";
            case VirtualAllocFailed: return "VirtualAlloc failed";
            case VirtualFreeFailed: return "VirtualFree failed";
            case VirtualProtectFailed: return "VirtualProtect failed";
            case VirtualAllocReturnedNull: return "VirtualAlloc returned null";
            case VirtualLockFailed: return "VirtualLock failed";
            case VirtualUnlockFailed: return "VirtualUnlock failed";
        }
        return "Unknown error";
    }
#else
    static const char* vmem_get_error_message(s32 _) { return "<Error messages disabled>"; }
#endif

    // Cached global page size.
    static vmem_size_t s_vmem_page_size              = 0;
    static vmem_size_t s_vmem_allocation_granularity = 0;

    vmem_size_t vmem_init(void)
    {
        // Note: this will be 2 syscalls on windows.
        s_vmem_page_size              = vmem_query_page_size();
        s_vmem_allocation_granularity = vmem_query_allocation_granularity();
        return s_vmem_page_size;
    }

    vmem_size_t vmem_get_page_size(void) { return s_vmem_page_size; }
    vmem_size_t vmem_get_allocation_granularity(void) { return s_vmem_allocation_granularity; }

    ptr_t vmem_align_forward(const ptr_t address, const s32 align)
    {
        if (!vmem_check(align == 0, AlignmentCannotBeZero))
            return 0;
        if (!vmem_check((align & (align - 1)) != 0, AlignmentHasToBePowerOf2))
            return 0;
        return vmem_align_forward_fast(address, align);
    }

    ptr_t vmem_align_backward(const ptr_t address, const s32 align)
    {
        if (!vmem_check(align == 0, AlignmentCannotBeZero))
            return 0;
        if (!vmem_check((align & (align - 1)) != 0, AlignmentHasToBePowerOf2))
            return 0;
        return vmem_align_backward_fast(address, align);
    }

    vmem_result_t vmem_is_aligned(const ptr_t address, const s32 align)
    {
        if (align == 0)
            return {vmem_result_t::Error};
        if ((align & (align - 1)) != 0)
            return {vmem_result_t::Error};
        return vmem_is_aligned_fast(address, align);
    }

    const char* vmem_get_protect_name(const vmem_protect_t protect)
    {
        switch (protect.value)
        {
            case vmem_protect_t::Invalid: return "INVALID";
            case vmem_protect_t::NoAccess: return "NoAccess";
            case vmem_protect_t::Read: return "Read";
            case vmem_protect_t::ReadWrite: return "ReadWrite";
            case vmem_protect_t::Execute: return "Execute";
            case vmem_protect_t::ExecuteRead: return "ExecuteRead";
            case vmem_protect_t::ExecuteReadWrite: return "ExecuteReadWrite";
        }
        return "<Unknown>";
    }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Windows backend implementation
//
#if defined(VMEM_PLATFORM_WIN32)
    static const DWORD s_protect_array[] = {0xffffffff, PAGE_NOACCESS, PAGE_READONLY, PAGE_READWRITE, PAGE_EXECUTE, PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE};
    static DWORD       vmem__win32_protect(const vmem_protect_t protect)
    {
        DWORD const protect_win = s_protect_array[protect.value];
        if (protect_win == 0xffffffff)
        {
            InvalidProtectMode;
            return {vmem_result_t::Error};
        }
        return protect_win;
    }

    static vmem_protect_t vmem__protect_from_win32(const DWORD protect)
    {
        switch (protect)
        {
            case PAGE_NOACCESS: return {vmem_protect_t::NoAccess};
            case PAGE_READONLY: return {vmem_protect_t::Read};
            case PAGE_READWRITE: return {vmem_protect_t::ReadWrite};
            case PAGE_EXECUTE: return {vmem_protect_t::Execute};
            case PAGE_EXECUTE_READ: return {vmem_protect_t::ExecuteRead};
            case PAGE_EXECUTE_READWRITE: return {vmem_protect_t::ExecuteReadWrite};
        }
        InvalidProtectMode;
        return {vmem_protect_t::Invalid};
    }

    void* vmem_alloc_protect(const vmem_size_t num_bytes, const vmem_protect_t protect)
    {
        if (!vmem_check(num_bytes == 0, CannotAllocateMemoryBlockWithSize0Bytes))
            return nullptr;

        const DWORD protect_win32 = vmem__win32_protect(protect);
        if (protect_win32)
        {
            LPVOID address = VirtualAlloc(NULL, (SIZE_T)num_bytes, MEM_RESERVE, protect_win32);
            if (!vmem_check(address == NULL, VirtualAllocReturnedNull))
                return nullptr;
            // Note: memory is initialized to zero.
            return address;
        }
        return nullptr;
    }

    vmem_result_t vmem_dealloc(void* ptr, const vmem_size_t num_allocated_bytes)
    {
        if (!vmem_check(ptr == 0, PtrCannotBeNull))
            return {vmem_result_t::Error};
        if (!vmem_check(num_allocated_bytes == 0, CannotDeallocAMemoryBlockOfSize0))
            return {vmem_result_t::Error};

        const BOOL result = VirtualFree(ptr, 0, MEM_RELEASE);
        if (!vmem_check(result == 0, VirtualFreeFailed))
            return {vmem_result_t::Error};
        return result ? vmem_result_t{vmem_result_t::Success} : vmem_result_t{vmem_result_t::Error};
    }

    vmem_result_t vmem_commit_protect(void* ptr, const vmem_size_t num_bytes, const vmem_protect_t protect)
    {
        if (!vmem_check(ptr == 0, PtrCannotBeNull))
            return {vmem_result_t::Error};
        if (!vmem_check(num_bytes == 0, SizeCannotBe0))
            return {vmem_result_t::Error};

        const LPVOID result = VirtualAlloc(ptr, num_bytes, MEM_COMMIT, vmem__win32_protect(protect));
        if (!vmem_check(result == 0, VirtualAllocFailed))
            return {vmem_result_t::Error};
        return {vmem_result_t::Success};
    }

    vmem_result_t vmem_decommit(void* ptr, const vmem_size_t num_bytes)
    {
        if (!vmem_check(ptr == 0, PtrCannotBeNull))
            return {vmem_result_t::Error};
        if (!vmem_check(num_bytes == 0, SizeCannotBe0))
            return {vmem_result_t::Error};

        const BOOL result = VirtualFree(ptr, num_bytes, MEM_DECOMMIT);
        if (!vmem_check(result == 0, VirtualFreeFailed))
            return {vmem_result_t::Error};
        return {vmem_result_t::Success};
    }

    vmem_result_t vmem_protect(void* ptr, const vmem_size_t num_bytes, const vmem_protect_t protect)
    {
        if (!vmem_check(ptr == 0, PtrCannotBeNull))
            return {vmem_result_t::Error};
        if (!vmem_check(num_bytes == 0, SizeCannotBe0))
            return {vmem_result_t::Error};

        DWORD      old_protect = 0;
        const BOOL result      = VirtualProtect(ptr, num_bytes, vmem__win32_protect(protect), &old_protect);
        if (!vmem_check(result == 0, VirtualProtectFailed))
            return {vmem_result_t::Error};
        return {vmem_result_t::Success};
    }

    vmem_size_t vmem_query_page_size(void)
    {
        SYSTEM_INFO system_info = {0};
        GetSystemInfo(&system_info);
        return system_info.dwPageSize;
    }

    vmem_size_t vmem_query_allocation_granularity(void)
    {
        SYSTEM_INFO system_info = {0};
        GetSystemInfo(&system_info);
        return system_info.dwAllocationGranularity;
    }

    vmem_usage_t vmem_query_usage_status(void)
    {
        MEMORYSTATUS status = {0};
        GlobalMemoryStatus(&status);

        vmem_usage_t usage_status         = {0};
        usage_status.total_physical_bytes = status.dwTotalPhys;
        usage_status.avail_physical_bytes = status.dwAvailPhys;

        return usage_status;
    }

    vmem_result_t vmem_lock(void* ptr, const vmem_size_t num_bytes)
    {
        if (!vmem_check(ptr == 0, PtrCannotBeNull))
            return {vmem_result_t::Error};
        if (!vmem_check(num_bytes == 0, SizeCannotBe0))
            return {vmem_result_t::Error};

        const BOOL result = VirtualLock(ptr, num_bytes);
        if (!vmem_check(result == 0, VirtualLockFailed))
            return {vmem_result_t::Error};
        return {vmem_result_t::Success};
    }

    vmem_result_t vmem_unlock(void* ptr, const vmem_size_t num_bytes)
    {
        if (!vmem_check(ptr == 0, PtrCannotBeNull))
            return {vmem_result_t::Error};
        if (!vmem_check(num_bytes == 0, SizeCannotBe0))
            return {vmem_result_t::Error};

        const BOOL result = VirtualUnlock(ptr, num_bytes);
        if (!vmem_check(result == 0, VirtualUnlockFailed))
            return {vmem_result_t::Error};
        return {vmem_result_t::Success};
    }

#endif // defined(VMEM_PLATFORM_WIN32)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MacOS backend implementation
//
#if defined(VMEM_PLATFORM_MAC)
    static const s32 s_protect_array[] = {-1, PROT_NONE, PROT_READ, PROT_READ | PROT_WRITE, PROT_EXEC, PROT_EXEC | PROT_READ, PROT_EXEC | PROT_READ | PROT_WRITE};

    static s32 vmem__mac_protect(const vmem_protect_t protect)
    {
        s32 const protect_mac = s_protect_array[protect.value];
        if (protect_mac == -1)
        {
            vmem_check(false, InvalidProtectMode);
            return 0;
        }
        return protect_mac;
    }

    static vmem_protect_t vmem__protect_from_mac(const s32 protect)
    {
        switch (protect)
        {
            case PROT_NONE: return {vmem_protect_t::NoAccess};
            case PROT_READ: return {vmem_protect_t::Read};
            case PROT_READ | PROT_WRITE: return {vmem_protect_t::ReadWrite};
            case PROT_EXEC: return {vmem_protect_t::Execute};
            case PROT_EXEC | PROT_READ: return {vmem_protect_t::ExecuteRead};
            case PROT_EXEC | PROT_READ | PROT_WRITE: return {vmem_protect_t::ExecuteReadWrite};
        }
        vmem_check(false, InvalidProtectMode);
        return {vmem_protect_t::Invalid};
    }

    void* vmem_alloc_protect(const vmem_size_t num_bytes, const vmem_protect_t protect)
    {
        if (!vmem_check(num_bytes == 0, CannotAllocateMemoryBlockWithSize0Bytes))
            return nullptr;

        const s32 protect_mac = vmem__mac_protect(protect);
        if (protect_mac)
        {
            void* address = mmap(nullptr, num_bytes, protect_mac, MAP_PRIVATE | MAP_ANON, -1, 0);
            if (!vmem_check(address == MAP_FAILED, FailedToFormatError))
                return nullptr;
            return address;
        }

        return 0;
    }

    vmem_result_t vmem_dealloc(void* ptr, const vmem_size_t num_allocated_bytes)
    {
        if (!vmem_check(ptr == 0, PtrCannotBeNull))
            return {vmem_result_t::Error};
        if (!vmem_check(num_allocated_bytes == 0, CannotDeallocAMemoryBlockOfSize0))
            return {vmem_result_t::Error};

        const s32 result = munmap(ptr, num_allocated_bytes);
        if (!vmem_check(result == -1, FailedToFormatError))
            return {vmem_result_t::Error};
        return {vmem_result_t::Success};
    }

    vmem_result_t vmem_commit_protect(void* ptr, const vmem_size_t num_bytes, const vmem_protect_t protect)
    {
        if (!vmem_check(ptr == 0, PtrCannotBeNull))
            return {vmem_result_t::Error};
        if (!vmem_check(num_bytes == 0, SizeCannotBe0))
            return {vmem_result_t::Error};

        const s32 protect_mac = vmem__mac_protect(protect);
        if (protect_mac)
        {
            const s32 result = mprotect(ptr, num_bytes, protect_mac);
            if (!vmem_check(result == -1, FailedToFormatError))
                return {vmem_result_t::Error};
            return {vmem_result_t::Success};
        }

        return {vmem_result_t::Error};
    }

    vmem_result_t vmem_decommit(void* ptr, const vmem_size_t num_bytes)
    {
        if (!vmem_check(ptr == 0, PtrCannotBeNull))
            return {vmem_result_t::Error};
        if (!vmem_check(num_bytes == 0, SizeCannotBe0))
            return {vmem_result_t::Error};

        const s32 result = mprotect(ptr, num_bytes, PROT_NONE);
        if (!vmem_check(result == -1, FailedToFormatError))
            return {vmem_result_t::Error};
        return {vmem_result_t::Success};
    }

    vmem_result_t vmem_protect(void* ptr, const vmem_size_t num_bytes, const vmem_protect_t protect)
    {
        if (!vmem_check(ptr == 0, PtrCannotBeNull))
            return {vmem_result_t::Error};
        if (!vmem_check(num_bytes == 0, SizeCannotBe0))
            return {vmem_result_t::Error};

        const s32 protect_mac = vmem__mac_protect(protect);
        if (protect_mac)
        {
            const s32 result = mprotect(ptr, num_bytes, protect_mac);
            if (!vmem_check(result == -1, FailedToFormatError))
                return {vmem_result_t::Error};
            return {vmem_result_t::Success};
        }

        return {vmem_result_t::Error};
    }

    vmem_size_t vmem_query_page_size(void) { return (vmem_size_t)vm_page_size; }
    vmem_size_t vmem_query_allocation_granularity(void) { return (vmem_size_t)vm_page_size; }

    vmem_usage_t vmem_query_usage_status(void)
    {
        vmem_usage_t usage_status         = {0};
        usage_status.total_physical_bytes = 0;
        usage_status.avail_physical_bytes = 0;

        mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
        vm_statistics64_data_t vm_stat;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO, (host_info_t)&vm_stat, &count) == KERN_SUCCESS)
        {
            usage_status.total_physical_bytes = (vmem_size_t)vm_stat.wire_count + (vmem_size_t)vm_stat.active_count + (vmem_size_t)vm_stat.inactive_count + (vmem_size_t)vm_stat.free_count;
            usage_status.avail_physical_bytes = (vmem_size_t)vm_stat.free_count;
        }

        return usage_status;
    }

    vmem_result_t vmem_lock(void* ptr, const vmem_size_t num_bytes)
    {
        if (!vmem_check(ptr == 0, PtrCannotBeNull))
            return {vmem_result_t::Error};
        if (!vmem_check(num_bytes == 0, SizeCannotBe0))
            return {vmem_result_t::Error};

        const s32 result = mlock(ptr, num_bytes);
        if (!vmem_check(result == -1, VirtualLockFailed))
            return {vmem_result_t::Error};
        return {vmem_result_t::Success};
    }

    vmem_result_t vmem_unlock(void* ptr, const vmem_size_t num_bytes)
    {
        if (!vmem_check(ptr == 0, PtrCannotBeNull))
            return {vmem_result_t::Error};
        if (!vmem_check(num_bytes == 0, SizeCannotBe0))
            return {vmem_result_t::Error};

        const s32 result = munlock(ptr, num_bytes);
        if (!vmem_check(result == -1, VirtualUnlockFailed))
            return {vmem_result_t::Error};
        return {vmem_result_t::Success};
    }

#endif

    static u32 s_pagesize = 0;
    bool       vmem_t::reserve(u64 address_range, u32& page_size, u32 reserve_flags, void*& baseptr)
    {
        page_size = s_pagesize;
        baseptr   = vmem_alloc_protect(address_range, {vmem_protect_t::ReadWrite});
        return baseptr != nullptr;
    }

    bool vmem_t::release(void* baseptr, u64 address_range) { return vmem_dealloc(baseptr, address_range).value == vmem_result_t::Success; }
    bool vmem_t::commit(void* page_address, u32 page_size, u32 page_count) { return vmem_commit_protect(page_address, (u64)page_size * page_count, {vmem_protect_t::ReadWrite}).value == vmem_result_t::Success; }
    bool vmem_t::decommit(void* baseptr, u32 page_size, u32 page_count) { return vmem_decommit(baseptr, (u64)page_size * page_count).value == vmem_result_t::Success; }

    bool vmem_t::initialize()
    {
        s_pagesize = (u32)vmem_init();
        return true;
    }

}; // namespace ncore
