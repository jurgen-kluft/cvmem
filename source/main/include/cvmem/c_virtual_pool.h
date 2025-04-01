#ifndef __C_VMEM_VIRTUAL_POOL_H__
#define __C_VMEM_VIRTUAL_POOL_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "cbase/c_allocator.h"

namespace ncore
{
    namespace nvmem
    {
        template <typename T> class pool_t : public ncore::pool_t<T>
        {
            u8* m_baseptr;     // memory base pointer
            u32 m_item_sizeof; // the size of an item in bytes
            u32 m_item_count;  // current number of items that are used
            u32 m_item_cap;    // maximum number of items that can be used
            u32 m_free_index;  // index of the first free item
            u32 m_free_head;   // index of the first free item in the free list

        public:
            pool_t();

            // e.g: setup(32768, 16777216);
            bool setup(u32 initial_item_count, u32 maximum_item_count);
            bool teardown();

            inline u32 capacity() const { return m_item_cap; }
            inline u32 size() const { return m_item_count; }

            inline T*       ptr() { return (T*)m_baseptr; }
            inline T const* ptr() const { return (T*)m_baseptr; }
            inline T*       ptr_at(u32 index) { return (T*)(m_baseptr + index * m_item_sizeof); }
            inline T const* ptr_at(u32 index) const { return (T const*)(m_baseptr + index * m_item_sizeof); }
            inline u32      idx_of(T const* item) const { return (u32)((u8*)item - m_baseptr) / m_item_sizeof; }

        protected:
            virtual u32   v_allocsize() const final;
            virtual void* v_allocate() final;
            virtual void  v_deallocate(void*) final;

            virtual void* v_idx2ptr(u32 index) final;
            virtual u32   v_ptr2idx(void const* ptr) const final;
        };
    } // namespace nvmem
}; // namespace ncore

#include "cvmem/private/c_virtual_pool_inline.h"

#endif /// __C_VMEM_VIRTUAL_ARRAY_H__
