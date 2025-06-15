#include "cbase/c_allocator.h"
#include "cbase/c_integer.h"
#include "cbase/c_memory.h"

#include "cunittest/cunittest.h"

#include "cvmem/c_virtual_arena.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(virtual_arena)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP()
        {
            // Setup the arenas with a default configuration
            ArenasSetup(32, 1024);
        }

        UNITTEST_FIXTURE_TEARDOWN()
        {
            // Teardown the arenas
            ArenasTeardown();
        }

        UNITTEST_TEST(new_arena)
        {
            arena_t* arena = ArenaAlloc(1024 << ARENA_DEFAULT_PAGESIZE_SHIFT, 256 << ARENA_DEFAULT_PAGESIZE_SHIFT);
            ASSERT(arena != nullptr);
            ASSERT(ArenaIsValid(arena));
            ASSERT(ArenaPos(arena) == 0);
            ASSERT(arena->CapacityReserved >= 1024 << ARENA_DEFAULT_PAGESIZE_SHIFT);
            ASSERT(arena->CapacityCommited >= 256 << ARENA_DEFAULT_PAGESIZE_SHIFT);
            ArenaRelease(arena);
        }

        UNITTEST_TEST(init_use_exit)
        {
            arena_t* arena = ArenaAlloc(1024 << ARENA_DEFAULT_PAGESIZE_SHIFT, 256 << ARENA_DEFAULT_PAGESIZE_SHIFT);
            ASSERT(arena != nullptr);
            ASSERT(ArenaIsValid(arena));

            // Should be able to write to that block of memory now
            // Let's do a memset
            nmem::memset(ArenaPush(arena, 128), 0xCD, 128);
            ASSERT(ArenaPos(arena) == 128);

            // Pop the last 64 bytes
            ArenaPop(arena, 64);
            ASSERT(ArenaPos(arena) == 64);

            // Clear the arena, keeping the commited memory
            ArenaClear(arena, 256 << ARENA_DEFAULT_PAGESIZE_SHIFT);
            ASSERT(ArenaPos(arena) == 0);

            ArenaRelease(arena);
        }
    }
}
UNITTEST_SUITE_END
