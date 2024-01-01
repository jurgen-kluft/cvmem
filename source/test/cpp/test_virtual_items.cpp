#include "cbase/c_allocator.h"
#include "cbase/c_integer.h"
#include "cbase/c_memory.h"

#include "cunittest/cunittest.h"

#include "cvmem/c_virtual_memory.h"
#include "cvmem/c_virtual_items.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(virtual_items)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP() {}

        UNITTEST_FIXTURE_TEARDOWN() {}

        // 20 bytes
        struct entity_t
        {
            f32  m_pos[3];
            f32  m_speed;
            bool m_alive;
        };

        UNITTEST_TEST(init_exit)
        {
            virtual_items_t array;
            array.init(sizeof(entity_t), 4096, 65536);

            array.exit();
        }

        UNITTEST_TEST(init_use_exit)
        {
            virtual_items_t array;
            array.init(sizeof(entity_t), 4096, 65536);

            // Should be able to write to that block of memory now
            // Let's do a memset
            nmem::memset(array.ptr_at<void>(0), 0xCDCDCDCD, sizeof(entity_t) * 4096);
            entity_t* e = array.ptr_at<entity_t>(0);

            array.exit();
        }
    }
}
UNITTEST_SUITE_END
