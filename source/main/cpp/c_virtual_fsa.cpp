#include "ccore/c_target.h"
#include "ccore/c_debug.h"

#include "cvmem/c_virtual_memory.h"
#include "cvmem/c_virtual_fsa.h"

namespace ncore
{
    /*
        virtual bool reserve(u64 address_range, u32& page_size, u32 attributes, void*& baseptr);
        virtual bool release(void* baseptr, u64 address_range);

        virtual bool commit(void* page_address, u32 page_size, u32 page_count);
        virtual bool decommit(void* page_address, u32 page_size, u32 page_count);
    */

    vmem_fsa_t::vmem_fsa_t()
        : m_baseptr(nullptr)
        , m_item_size(0)
        , m_item_count(0)
        , m_item_cap(0)
        , m_free_index(0)
        , m_free_head(0xffffffff)
    {
    }

    static inline u32 s_number_of_pages(u32 item_size, u32 item_count, s8 page_size_shift) { return ((item_count * item_size) + ((1 << page_size_shift) - 1)) >> page_size_shift; }

    bool vmem_fsa_t::init(u32 item_size, u32 initial_item_count, u32 maximum_item_count)
    {
        m_baseptr                       = nullptr;
        u32       page_size             = 0;
        u64 const maximum_address_range = maximum_item_count * item_size;
        if (!vmem_t::reserve(maximum_address_range, page_size, 0, m_baseptr))
            return false;

        m_item_cap   = 0;
        m_item_size  = item_size;
        m_item_count = initial_item_count;
        u32 const page_max   = (item_size * maximum_item_count) / vmem_get_page_size();

        u32 page_com = s_number_of_pages(item_size, initial_item_count, page_size);
        if (page_com > page_max)
        {
            page_com = page_max;
        }

        if (page_com > 0)
        {
            if (!vmem_t::commit(m_baseptr, page_size, page_com))
                return false;

            m_item_cap = (page_com * vmem_get_page_size()) / m_item_size;
        }

        return true;
    }

    bool vmem_fsa_t::exit()
    {
        u32 const page_max   = ((m_item_size * m_item_cap) + vmem_get_page_size() - 1) / vmem_get_page_size();
        if (!vmem_t::release(m_baseptr, page_max * vmem_get_page_size()))
            return false;
        return true;
    }

    u32 vmem_fsa_t::v_allocsize() const { return m_item_size; }

    void* vmem_fsa_t::v_allocate()
    {
        if (m_free_head != 0xffffffff)
        {
            u32 const index = m_free_head;
            u32*      p     = (u32*)idx2ptr(index);
            m_free_head     = *p;
            m_item_count++;
            return p;
        }
        else
        {
            if (m_free_index < m_item_cap)
            {
                u32 const index = m_free_index++;
                m_item_count++;
                return idx2ptr(index);
            }
        }
        return nullptr;
    }

    void vmem_fsa_t::v_deallocate(void* ptr)
    {
        const u32 index = ptr2idx(ptr);
        *(u32*)ptr      = m_free_head;
        m_free_head     = index;
        m_item_count--;
    }

    void* vmem_fsa_t::v_idx2ptr(u32 index) const { return (void*)((u8*)m_baseptr + index * m_item_size); }
    u32   vmem_fsa_t::v_ptr2idx(void* ptr) const { return (u32)(((u8*)ptr - (u8*)m_baseptr) / m_item_size); }

} // namespace ncore
