#include "ccore/c_target.h"
#include "ccore/c_debug.h"

#include "cvmem/c_virtual_memory.h"
#include "cvmem/c_virtual_items.h"

namespace ncore
{
    /*
        virtual bool reserve(u64 address_range, u32& page_size, u32 attributes, void*& baseptr);
        virtual bool release(void* baseptr, u64 address_range);

        virtual bool commit(void* page_address, u32 page_size, u32 page_count);
        virtual bool decommit(void* page_address, u32 page_size, u32 page_count);
    */

    virtual_items_t::virtual_items_t()
        : m_item_size(0)
        , m_item_cap(0)
        , m_free_index(0)
        , m_free_head(0xffffffff)
        , m_item_count(0)
        , m_page_size(0)
        , m_page_com(0)
        , m_page_max(0)
        , m_baseptr(nullptr)
    {
    }

    static inline u32 number_of_pages(u32 item_size, u32 item_count, u32 page_size) { return ((item_count * item_size) + (page_size - 1)) / page_size; }

    bool virtual_items_t::init(u32 item_size, u32 initial_item_count, u32 maximum_item_count)
    {
        m_baseptr                       = nullptr;
        u64 const maximum_address_range = maximum_item_count * item_size;
        if (!vmem_t::reserve(maximum_address_range, m_page_size, 0, m_baseptr))
            return false;

        m_item_cap   = 0;
        m_item_size  = item_size;
        m_item_count = initial_item_count;
        m_page_com   = number_of_pages(item_size, initial_item_count, m_page_size);
        m_page_max   = number_of_pages(item_size, maximum_item_count, m_page_size);

        if (m_page_com > m_page_max)
        {
            m_page_com = m_page_max;
        }

        if (m_page_com > 0)
        {
            if (!vmem_t::commit(m_baseptr, m_page_size, m_page_com))
                return false;

            m_item_cap = (m_page_com * m_page_size) / m_item_size;
        }

        return true;
    }

    bool virtual_items_t::exit()
    {
        if (!vmem_t::release(m_baseptr, m_page_max * m_page_size))
            return false;
        return true;
    }

    void virtual_items_t::set_capacity(u32 item_count)
    {
        if (item_count > m_item_count)
        {
            // Grow
            u32 const page_com = number_of_pages(m_item_size, item_count, m_page_size);
            if (page_com > m_page_com && page_com <= m_page_max)
            {
                u32 const page_cnt = page_com - m_page_com;
                void*     baseptr  = (void*)((u8*)m_baseptr + (m_page_com * m_page_size));
                vmem_t::commit(baseptr, m_page_size, page_cnt);
                m_page_com = page_com;
                m_item_cap = (m_page_com * m_page_size) / m_item_size;
            }
        }
        else if (item_count < m_item_count)
        {
            // Shrink
            u32 const page_com = number_of_pages(m_item_size, item_count, m_page_size);
            if (page_com < m_page_com)
            {
                u32 const page_cnt = m_page_com - page_com;
                void*     baseptr  = (void*)((u8*)m_baseptr + (page_com * m_page_size));
                vmem_t::decommit(baseptr, m_page_size, page_cnt);
                m_page_com = page_com;
                m_item_cap = (m_page_com * m_page_size) / m_item_size;
            }
        }
    }

    void* virtual_items_t::allocate()
    {
        if (m_free_head != 0xffffffff)
        {
            u32 const index = m_free_head;
            u32*      p     = ptr_at<u32>(index);
            m_free_head     = *p;
            m_item_count++;
            return p;
        }
        else
        {
            if (m_free_index >= m_item_cap)
            {
                // Grow, new capacity is 1.5 times the current capacity
                set_capacity((m_item_count * 3) / 2);
            }

            if (m_free_index < m_item_cap)
            {
                u32 const index = m_free_index++;
                m_item_count++;
                return ptr_at<void>(index);
            }
        }
        return nullptr;
    }

    void virtual_items_t::deallocate(void* ptr)
    {
        const u32 index = index_of(ptr);
        *(u32*)ptr      = m_free_head;
        m_free_head     = index;
        m_item_count--;
    }

} // namespace ncore
