#include "xbase/x_target.h"
#include "xbase/x_debug.h"

#include "xvmem/x_virtual_memory.h"
#include "xvmem/x_virtual_array.h"

namespace ncore
{
    /*
        virtual bool reserve(u64 address_range, u32& page_size, u32 attributes, void*& baseptr);
        virtual bool release(void* baseptr, u64 address_range);

        virtual bool commit(void* page_address, u32 page_size, u32 page_count);
        virtual bool decommit(void* page_address, u32 page_size, u32 page_count);
    */

    virtual_array_t::virtual_array_t()
    {
        m_item_size  = 0;
        m_item_count = 0;
        m_page_com   = 0;
        m_page_max   = 0;
        m_baseptr    = nullptr;
    }

    bool virtual_array_t::init(u32 item_size, u64 initial_item_count, u64 maximum_address_range)
    {
        m_baseptr = nullptr;
        u32 page_size = 0;
        if (!vmem->reserve(maximum_address_range, page_size, 0, m_baseptr))
            return false;

        m_item_size  = item_size;
        m_item_count = initial_item_count;
        m_page_com   = ((initial_item_count * item_size) + (page_size - 1)) / page_size;
        m_page_max   = (maximum_address_range + (page_size - 1)) / page_size;

        if (m_page_com > m_page_max)
        {
            m_page_com = m_page_max;
        }

        if (m_page_com > 0)
        {
            if (!vmem->commit(m_baseptr, page_size, m_page_com))
                return false;
        }

        return true;
    }

    bool virtual_array_t::exit()
    {
        u32 const page_size = 65536;
        if (!vmem->release(m_baseptr, m_page_max * page_size))
            return false;
        return true;
    }

    void virtual_array_t::set_size(u32 item_count)
    {
        if (item_count > m_item_count)
        {
            // Grow
            u32 const page_size = 65536;
            u32 const page_com = ((item_count * m_item_size) + (page_size - 1)) / page_size;
            if (page_com > m_page_com && page_com < m_page_max)
            {
                u32 const page_cnt = page_com - m_page_com;
                void*     baseptr  = (void*)((u8*)m_baseptr + (m_page_com * page_size));
                vmem->commit(baseptr, page_size, page_cnt);
                m_page_com = page_com;
            }
        }
        else if (item_count < m_item_count)
        {
            // Shrink
            u32 const page_size = 65536;
            u32 const page_com = ((item_count * m_item_size) + (page_size - 1)) / page_size;
            if (page_com < m_page_com)
            {
                u32 const page_cnt = m_page_com - page_com;
                void*     baseptr  = (void*)((u8*)m_baseptr + (page_com * page_size));
                vmem->decommit(baseptr, page_size, page_cnt);
                m_page_com = page_com;
            }
        }
    }

} // namespace ncore
