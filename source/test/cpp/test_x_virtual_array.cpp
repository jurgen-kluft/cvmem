#include "cbase/c_allocator.h"
#include "cbase/c_integer.h"
#include "cbase/c_memory.h"

#include "cunittest/xunittest.h"

#include "cvmem/c_virtual_memory.h"
#include "cvmem/c_virtual_array.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(virtual_array)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP() { }

        UNITTEST_FIXTURE_TEARDOWN() {}

        struct entity_t{
            f32 m_pos[3];
            f32 m_speed;
            bool m_alive;
        };

        UNITTEST_TEST(init_exit)
        {
            u64   address_range = 1 * xGB;

            virtual_array_t array;
            array.init(sizeof(entity_t), 4096, address_range);

            array.exit();
        }

        UNITTEST_TEST(init_use_exit)
        {
            u64 const address_range = 4 * xGB;

            virtual_array_t array;
            array.init(sizeof(entity_t), 4096, address_range);

            // Should be able to write to that block of memory now
            // Let's do a memset
            nmem::memset(array.get(0), 0xCDCDCDCD, sizeof(entity_t) * 4096);
            entity_t* e = array.at<entity_t>(0);

            array.exit();
        }
    }
}
UNITTEST_SUITE_END
