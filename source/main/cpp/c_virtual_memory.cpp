#include "ccore/c_target.h"
#include "ccore/c_debug.h"
#include "ccore/c_allocator.h"

#include "cvmem/c_virtual_memory.h"

#if defined TARGET_MAC
#    include <sys/mman.h>
#endif
#if defined TARGET_PC
#    include "Windows.h"
#endif

namespace ncore
{
    class vmem_os_t : public vmem_t
    {
    public:
        virtual bool initialize(u32 pagesize);

        virtual bool reserve(u64 address_range, u32& page_size, u32 attributes, void*& baseptr);
        virtual bool release(void* baseptr, u64 address_range);

        virtual bool commit(void* page_address, u32 page_size, u32 page_count);
        virtual bool decommit(void* page_address, u32 page_size, u32 page_count);

    private:
        u32 m_pagesize;
    };

#if defined TARGET_MAC

#    define SYS_PAGE_SIZE 65536

    bool vmem_os_t::reserve(u64 address_range, u32& page_size, u32 reserve_flags, void*& baseptr)
    {
        page_size = m_pagesize;

        baseptr = mmap(nullptr, address_range, PROT_NONE, MAP_PRIVATE | MAP_ANON | reserve_flags, -1, 0);
        if (baseptr == MAP_FAILED)
            baseptr = nullptr;

        msync(baseptr, address_range, (MS_SYNC | MS_INVALIDATE));
        return baseptr != nullptr;
    }

    bool vmem_os_t::release(void* baseptr, u64 address_range)
    {
        msync(baseptr, address_range, MS_SYNC);
        s32 ret = munmap(baseptr, address_range);
        ASSERT(ret == 0); // munmap failed
        return ret == 0;
    }

    bool vmem_os_t::commit(void* page_address, u32 page_size, u32 page_count)
    {
        if (page_size == 0)
            page_size = SYS_PAGE_SIZE;
        u32 const commit_flags = MAP_FIXED | MAP_PRIVATE | MAP_ANON;
        mmap(page_address, page_size * page_count, PROT_READ | PROT_WRITE, commit_flags, -1, 0);
        s32 ret = msync(page_address, page_size * page_count, MS_SYNC | MS_INVALIDATE);
        return ret == 0;
    }

    bool vmem_os_t::decommit(void* page_address, u32 page_size, u32 page_count)
    {
        if (page_size == 0)
            page_size = SYS_PAGE_SIZE;
        u32 const commit_flags = MAP_FIXED | MAP_PRIVATE | MAP_ANON;
        mmap(page_address, page_size * page_count, PROT_NONE, commit_flags, -1, 0);
        s32 ret = msync(page_address, page_size * page_count, MS_SYNC | MS_INVALIDATE);
        return ret == 0;
    }

    bool vmem_os_t::initialize(u32 pagesize)
    {
        if (pagesize > 0)
        {
            m_pagesize = pagesize;
        }
        else
        {
            m_pagesize = SYS_PAGE_SIZE;
        }
        return true;
    }

    static vmem_os_t sVMem;
    vmem_t*          vmem = &sVMem;

#elif defined TARGET_PC

    bool vmem_os_t::reserve(u64 address_range, u32& page_size, u32 reserve_flags, void*& baseptr)
    {
        unsigned int allocation_type = MEM_RESERVE | reserve_flags;
        unsigned int protect         = 0;
        baseptr                      = ::VirtualAlloc(nullptr, (SIZE_T)address_range, allocation_type, protect);
        page_size                    = m_pagesize;
        return baseptr != nullptr;
    }

    bool vmem_os_t::release(void* baseptr, u64 address_range)
    {
        BOOL b = ::VirtualFree(baseptr, 0, MEM_RELEASE);
        return b;
    }

    bool vmem_os_t::commit(void* page_address, u32 page_size, u32 page_count)
    {
        unsigned int allocation_type = MEM_COMMIT;
        unsigned int protect         = PAGE_READWRITE;
        BOOL         success         = ::VirtualAlloc(page_address, page_size * page_count, allocation_type, protect) != nullptr;
        return success;
    }

    bool vmem_os_t::decommit(void* page_address, u32 page_size, u32 page_count)
    {
        unsigned int allocation_type = MEM_DECOMMIT;
        BOOL         b               = ::VirtualFree(page_address, page_size * page_count, allocation_type);
        return b;
    }

    bool vmem_os_t::initialize(u32 pagesize)
    {
        if (pagesize > 0)
        {
            m_pagesize = pagesize;
        }
        else
        {
            SYSTEM_INFO sysinfo;
            ::GetSystemInfo(&sysinfo);
            m_pagesize = sysinfo.dwPageSize;
        }
        return true;
    }

    static vmem_os_t sVMem;
    vmem_t*          vmem = &sVMem;

#else

#    error Unknown Platform/Compiler configuration for xvmem

#endif

#if !defined(VMEM_NO_ERROR_CHECKING)
    // clang-format off
#if !defined(VMEM_NO_ERROR_MESSAGES)
#define VMEM_ERROR_IF(cond, write_message) do { if(cond) { write_message; VMEM_ON_ERROR(#cond); return 0; } } while(0)
#else
#define VMEM_ERROR_IF(cond, write_message) do { if(cond) { VMEM_ON_ERROR(#cond); return 0; } } while(0)
#endif
// clang-format on
#else
#    define VMEM_ERROR_IF(cond, write_message) // Ignore
#endif

#if !defined(VMEM_ON_ERROR)
#    define VMEM_ON_ERROR(opt_string) ASSERT(0 && (opt_string))
#endif

#if !defined(VMEM_NO_ERROR_MESSAGES)
    static s32 vmem__g_error = 0;

    static void vmem__write_error(s32 error) { vmem__g_error = error; }

    const char* vmem_get_error_message(void)
    {
        switch (vmem__g_error)
        {
            case 0: return "No error";
        }
        return "<Unknown>";
    }
#else
#    define vmem__write_error(message) // Ignore

    const char* vmem_get_error(void) { return "<Error messages disabled>"; }
#endif

    // Cached global page size.
    static VMemSize vmem__g_page_size              = 0;
    static VMemSize vmem__g_allocation_granularity = 0;

    void vmem_init(void)
    {
        // Note: this will be 2 syscalls on windows.
        vmem__g_page_size              = vmem_query_page_size();
        vmem__g_allocation_granularity = vmem_query_allocation_granularity();
    }

    VMemSize vmem_get_page_size(void) { return vmem__g_page_size; }
    VMemSize vmem_get_allocation_granularity(void) { return vmem__g_allocation_granularity; }

    static const s32 AlignmentCannotBeZero    = 1;
    static const s32 AlignmentHasToBePowerOf2 = 2;

    ptr_t vmem_align_forward(const ptr_t address, const int align)
    {
        VMEM_ERROR_IF(align == 0, vmem__write_error(AlignmentCannotBeZero));
        VMEM_ERROR_IF((align & (align - 1)) != 0, vmem__write_error(AlignmentHasToBePowerOf2));
        return vmem_align_forward_fast(address, align);
    }

    ptr_t vmem_align_backward(const ptr_t address, const int align)
    {
        VMEM_ERROR_IF(align == 0, vmem__write_error(AlignmentCannotBeZero));
        VMEM_ERROR_IF((align & (align - 1)) != 0, vmem__write_error(AlignmentHasToBePowerOf2));
        return vmem_align_backward_fast(address, align);
    }

    VMemResult vmem_is_aligned(const ptr_t address, const int align)
    {
        if (align == 0)
            return 0;
        if ((align & (align - 1)) != 0)
            return 0;
        return vmem_is_aligned_fast(address, align);
    }

    const char* vmem_get_protect_name(const VMemProtect protect)
    {
        switch (protect)
        {
            case nVMemProtect::Invalid: return "INVALID";
            case nVMemProtect::NoAccess: return "NoAccess";
            case nVMemProtect::Read: return "Read";
            case nVMemProtect::ReadWrite: return "ReadWrite";
            case nVMemProtect::Execute: return "Execute";
            case nVMemProtect::ExecuteRead: return "ExecuteRead";
            case nVMemProtect::ExecuteReadWrite: return "ExecuteReadWrite";
        }
        return "<Unknown>";
    }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Windows backend implementation
//
#if defined(VMEM_PLATFORM_WIN32)
    static DWORD vmem__win32_protect(const VMemProtect protect)
    {
        switch (protect)
        {
            case nVMemProtect::NoAccess: return PAGE_NOACCESS;
            case nVMemProtect::Read: return PAGE_READONLY;
            case nVMemProtect::ReadWrite: return PAGE_READWRITE;
            case nVMemProtect::Execute: return PAGE_EXECUTE;
            case nVMemProtect::ExecuteRead: return PAGE_EXECUTE_READ;
            case nVMemProtect::ExecuteReadWrite: return PAGE_EXECUTE_READWRITE;
        }
        vmem__write_error_message("InvalidProtectMode.");
        return VMemResult_Error;
    }

    static VMemProtect vmem__protect_from_win32(const DWORD protect)
    {
        switch (protect)
        {
            case PAGE_NOACCESS: return nVMemProtect::NoAccess;
            case PAGE_READONLY: return nVMemProtect::Read;
            case PAGE_READWRITE: return nVMemProtect::ReadWrite;
            case PAGE_EXECUTE: return nVMemProtect::Execute;
            case PAGE_EXECUTE_READ: return nVMemProtect::ExecuteRead;
            case PAGE_EXECUTE_READWRITE: return nVMemProtect::ExecuteReadWrite;
        }
        vmem__write_error_message("InvalidProtectMode.");
        return VMemProtect_Invalid;
    }

#    if !defined(VMEM_NO_ERROR_MESSAGES)
    static void vmem__write_win32_error_message(void)
    {
        const DWORD result = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), vmem__g_error_message, sizeof(vmem__g_error_message), NULL);

        if (result == 0)
        {
            vmem__write_error_message("<FailedtoformatWin32Error>");
       }
        else
        {
            // Rewrite the last \n to zero
            vmem__g_error_message[(int)result - 1] = '\0';
        }
    }
#    endif

    void* vmem_alloc_protect(const VMemSize num_bytes, const VMemProtect protect)
    {
        VMEM_ERROR_IF(num_bytes == 0, vmem__write_error_message("CannotAllocateMemoryBlockWithSize0Bytes."));

        const DWORD protect_win32 = vmem__win32_protect(protect);
        if (protect_win32)
        {
            LPVOID address = VirtualAlloc(NULL, (SIZE_T)num_bytes, MEM_RESERVE, protect_win32);
            VMEM_ERROR_IF(address == NULL, vmem__write_win32_error_message());
            // Note: memory is initialized to zero.
            return address;
        }

        return 0;
    }

    VMemResult vmem_dealloc(void* ptr, const VMemSize num_allocated_bytes)
    {
        VMEM_ERROR_IF(ptr == 0, vmem__write_error_message("PtrCannotBeNull."));
        VMEM_ERROR_IF(num_allocated_bytes == 0, vmem__write_error_message("CannotDeallocAMemoryBlockOfSize0(num_allocated_bytesIs0)."));

        const BOOL result = VirtualFree(ptr, 0, MEM_RELEASE);
        VMEM_ERROR_IF(result == 0, vmem__write_win32_error_message());
        return VMemResult_Success;
    }

    VMemResult vmem_commit_protect(void* ptr, const VMemSize num_bytes, const VMemProtect protect)
    {
        VMEM_ERROR_IF(ptr == 0, vmem__write_error_message("PtrCannotBeNull."));
        VMEM_ERROR_IF(num_bytes == 0, vmem__write_error_message("SizeCannotBe0."));

        const LPVOID result = VirtualAlloc(ptr, num_bytes, MEM_COMMIT, vmem__win32_protect(protect));
        VMEM_ERROR_IF(result == 0, vmem__write_win32_error_message());
        return VMemResult_Success;
    }

    VMemResult vmem_decommit(void* ptr, const VMemSize num_bytes)
    {
        VMEM_ERROR_IF(ptr == 0, vmem__write_error_message("PtrCannotBeNull."));
        VMEM_ERROR_IF(num_bytes == 0, vmem__write_error_message("SizeCannotBe0."));

        const BOOL result = VirtualFree(ptr, num_bytes, MEM_DECOMMIT);
        VMEM_ERROR_IF(result == 0, vmem__write_win32_error_message());
        return VMemResult_Success;
    }

    VMemResult vmem_protect(void* ptr, const VMemSize num_bytes, const VMemProtect protect)
    {
        VMEM_ERROR_IF(ptr == 0, vmem__write_error_message("PtrCannotBeNull."));
        VMEM_ERROR_IF(num_bytes == 0, vmem__write_error_message("SizeCannotBe0."));

        DWORD      old_protect = 0;
        const BOOL result      = VirtualProtect(ptr, num_bytes, vmem__win32_protect(protect), &old_protect);
        VMEM_ERROR_IF(result == 0, vmem__write_win32_error_message());
        return VMemResult_Success;
    }

    VMemSize vmem_query_page_size(void)
    {
        SYSTEM_INFO system_info = {0};
        GetSystemInfo(&system_info);
        return system_info.dwPageSize;
    }

    VMemSize vmem_query_allocation_granularity(void)
    {
        SYSTEM_INFO system_info = {0};
        GetSystemInfo(&system_info);
        return system_info.dwAllocationGranularity;
    }

    VMemUsageStatus vmem_query_usage_status(void)
    {
        MEMORYSTATUS status = {0};
        GlobalMemoryStatus(&status);

        VMemUsageStatus usage_status      = {0};
        usage_status.total_physical_bytes = status.dwTotalPhys;
        usage_status.avail_physical_bytes = status.dwAvailPhys;

        return usage_status;
    }

    VMemResult vmem_lock(void* ptr, const VMemSize num_bytes)
    {
        VMEM_ERROR_IF(ptr == 0, vmem__write_error_message("PtrCannotBeNull."));
        VMEM_ERROR_IF(num_bytes == 0, vmem__write_error_message("SizeCannotBe0."));

        const BOOL result = VirtualLock(ptr, num_bytes);
        VMEM_ERROR_IF(result == 0, vmem__write_win32_error_message());
        return VMemResult_Success;
    }

    VMemResult vmem_unlock(void* ptr, const VMemSize num_bytes)
    {
        VMEM_ERROR_IF(ptr == 0, vmem__write_error_message("PtrCannotBeNull."));
        if (num_bytes == 0)
            return 0;

        const BOOL result = VirtualUnlock(ptr, num_bytes);
        VMEM_ERROR_IF(result == 0, vmem__write_win32_error_message());
        return VMemResult_Success;
    }

    VMemSize vmem_query_range_info(void* ptr, const VMemSize num_bytes, VMemRangeInfo* out_buf, const VMemSize buf_max_items)
    {
        VMEM_ERROR_IF(ptr == 0, vmem__write_error_message("PtrCannotBeNull."));
        VMEM_ERROR_IF(num_bytes == 0, vmem__write_error_message("SizeCannotBe0."));
        VMEM_ERROR_IF(out_buf == 0, vmem__write_error_message("OutBufferPtrCannotBeNull."));
        VMEM_ERROR_IF(buf_max_items == 0, vmem__write_error_message("OutBufferSizeCannotBe0."));

        size_t item_index = 0;
        for (size_t i = 0; i < num_bytes && item_index < buf_max_items;)
        {
            MEMORY_BASIC_INFORMATION info = {0};

            void*  p              = (void*)((uintptr_t)ptr + i);
            SIZE_T info_buf_bytes = VirtualQuery(p, &info, sizeof(info));
            VMEM_ERROR_IF(info_buf_bytes == 0, vmem__write_win32_error_message());

            DWORD protect = info.Protect;
            if (protect == 0)
                protect = info.AllocationProtect;

            VMemRangeInfo result_info = {0};
            result_info.ptr           = info.BaseAddress;
            result_info.size_bytes    = info.RegionSize;
            result_info.protect       = vmem__protect_from_win32(protect);
            result_info.is_commited   = info.State == MEM_COMMIT;
            out_buf[item_index]       = result_info;

            i += info.RegionSize;
            item_index++;
        }
        return item_index;
    }

#endif // defined(VMEM_PLATFORM_WIN32)

}; // namespace ncore
