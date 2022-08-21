#include "cbase/c_allocator.h"
#include "cbase/c_integer.h"
#include "cbase/c_memory.h"

#include "xunittest/xunittest.h"

#include "cvmem/c_virtual_memory.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(virtual_memory)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP() 
        { 
            vmem->initialize(); 
        }

        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(reserve_release)
        {
            u64   address_range = 4 * xGB;
            u32   pagesize;
            void* baseptr;
            CHECK_TRUE(vmem->reserve(address_range, pagesize, 0, baseptr));
            CHECK_TRUE(vmem->release(baseptr, address_range));
        }

        // virtual bool commit(void* address, u32 page_size, u32 page_count)   = 0;
        // virtual bool decommit(void* address, u32 page_size, u32 page_count) = 0;
        UNITTEST_TEST(commit_decommit)
        {
            u64   address_range = 4 * xGB;
            u32   pagesize;
            void* baseptr;
            CHECK_TRUE(vmem->reserve(address_range, pagesize, 0, baseptr));
            CHECK_TRUE(vmem->commit(baseptr, pagesize, 4));

            // Should be able to write to that block of memory now
            // Let's do a memset
            nmem::memset(baseptr, 0xCDCDCDCD, pagesize * 4);

            CHECK_TRUE(vmem->decommit(baseptr, pagesize, 4));
            CHECK_TRUE(vmem->release(baseptr, address_range));
        }
    }
}
UNITTEST_SUITE_END
