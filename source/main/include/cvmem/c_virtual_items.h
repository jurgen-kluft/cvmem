#ifndef __C_VMEM_VIRTUAL_ARRAY_H__
#define __C_VMEM_VIRTUAL_ARRAY_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

namespace ncore
{
    class virtual_items_t
    {
        u32   m_item_size;  // the size of an item in bytes
        u32   m_item_cap;   // maximum number of items that can be used
        u32   m_free_index; // index of the first free item
        u32   m_free_head;  // index of the first free item in the free list
        u32   m_item_count; // current number of items that are used
        u32   m_page_size;  // size of a page in bytes
        u16   m_page_com;   // number of committed pages
        u16   m_page_max;   // maximum number of pages
        void* m_baseptr;    // memory base pointer

    public:
        virtual_items_t();

        // e.g: init(sizeof(entity_t), 32768, 16777216);
        bool init(u32 item_size, u32 initial_item_count, u32 maximum_item_count);
        bool exit();

        template <typename T> inline T* ptr_at(u32 i)
        {
            ASSERT(i < m_item_cap);
            return (T*)((u8*)m_baseptr + (i * m_item_size));
        }

        inline u32 index_of(void* ptr) const
        {
            ASSERT(ptr >= m_baseptr);
            ASSERT(ptr < (u8*)m_baseptr + (m_item_cap * m_item_size));
            return (u32)(((u8*)ptr - (u8*)m_baseptr) / m_item_size);
        }

        void       set_capacity(u32 item_cap);
        inline u32 get_capacity() const { return m_item_cap; }
        inline u32 get_size() const { return m_item_count; }

        template <typename T> inline T*   alloc() { return (T*)allocate(); }
        template <typename T> inline void free(T* ptr) { deallocate(ptr); }

        void* allocate();
        void  deallocate(void* ptr);
    };

}; // namespace ncore

#endif /// __C_VMEM_VIRTUAL_ARRAY_H__