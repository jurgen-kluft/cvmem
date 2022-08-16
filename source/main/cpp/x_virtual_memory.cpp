#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_allocator.h"

#include "xvmem/x_virtual_memory.h"

#if defined TARGET_MAC
#include <sys/mman.h>
#endif
#if defined TARGET_PC
#include "Windows.h"
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

#define SYS_PAGE_SIZE 65536

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
        if (page_size == 0) page_size = SYS_PAGE_SIZE;
        u32 const commit_flags = MAP_FIXED | MAP_PRIVATE | MAP_ANON;
        mmap(page_address, page_size * page_count, PROT_READ | PROT_WRITE, commit_flags, -1, 0);
        s32 ret =msync(page_address, page_size * page_count, MS_SYNC | MS_INVALIDATE);
        return ret == 0;
    }

    bool vmem_os_t::decommit(void* page_address, u32 page_size, u32 page_count)
    {
        if (page_size == 0) page_size = SYS_PAGE_SIZE;
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
    vmem_t* vmem = &sVMem;

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
    vmem_t* vmem = &sVMem;

#else

#error Unknown Platform/Compiler configuration for xvmem

#endif

}; // namespace ncore
