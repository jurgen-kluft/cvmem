#ifndef __C_VMEM_VIRTUAL_ARRAY_H__
#define __C_VMEM_VIRTUAL_ARRAY_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "cbase/c_allocator.h"

namespace ncore
{
    class vmem_fsa_t final : public fsa_t, public dexer_t
    {
        void* m_baseptr;         // memory base pointer
        u32   m_item_size;       // the size of an item in bytes
        u32   m_item_count;      // current number of items that are used
        u32   m_item_cap;        // maximum number of items that can be used
        u32   m_free_index;      // index of the first free item
        u32   m_free_head;       // index of the first free item in the free list

    public:
        vmem_fsa_t();

        // e.g: init(sizeof(entity_t), 32768, 16777216);
        bool init(u32 item_size, u32 initial_item_count, u32 maximum_item_count);
        bool exit();

        inline u32 get_capacity() const { return m_item_cap; }
        inline u32 get_size() const { return m_item_count; }

    protected:
        virtual u32   v_allocsize() const final;
        virtual void* v_allocate() final;
        virtual void  v_deallocate(void*) final;

        virtual void* v_idx2ptr(u32 index) const final;
        virtual u32   v_ptr2idx(void* ptr) const final;
    };

}; // namespace ncore

#endif /// __C_VMEM_VIRTUAL_ARRAY_H__
