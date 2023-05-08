#ifndef __C_VMEM_VIRTUAL_ARRAY_H__
#define __C_VMEM_VIRTUAL_ARRAY_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

namespace ncore
{
    class virtual_array_t
    {
    public:
        virtual_array_t();
        
        // e.g: init(sizeof(entity_t), 4096, xGB);
        bool init(u32 item_size, u64 initial_item_count, u64 maximum_address_range);
        bool exit();

        template<typename T>
        inline T* at(s32 i) { return (T*)m_baseptr + (i * m_item_size); }
        inline u8* get(s32 i) { return (u8*)m_baseptr + (i * m_item_size); }
        
        inline u32 get_size() const { return m_item_count; }
        void set_size(u32 item_count);

    protected:
        u32 m_item_size;    // the size of an item in bytes
        u32 m_item_count;   // number of items that can be used
        u32 m_page_com;     // number of committed pages
        u32 m_page_max;     // maximum number of pages
        void* m_baseptr;    // 
    };

}; // namespace ncore

#endif /// __C_VMEM_VIRTUAL_ARRAY_H__