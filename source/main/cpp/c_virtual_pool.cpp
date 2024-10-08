#include "ccore/c_target.h"
#include "ccore/c_debug.h"

#include "cvmem/c_virtual_memory.h"
#include "cvmem/c_virtual_pool.h"

namespace ncore
{
    namespace nvmem
    {
        pool_t::pool_t()
            : m_baseptr(nullptr)
            , m_item_size(0)
            , m_item_count(0)
            , m_item_cap(0)
            , m_free_index(0)
            , m_free_head(0xffffffff)
        {
        }

        static inline u32 s_number_of_pages(u32 item_size, u32 item_count, s8 page_size_shift) { return ((item_count * item_size) + ((1 << page_size_shift) - 1)) >> page_size_shift; }

        bool pool_t::init(u32 item_size, u32 item_align, u32 initial_item_count, u32 maximum_item_count)
        {
            m_baseptr = nullptr;

            const u64 maximum_address_range = maximum_item_count * item_size;
            void*     baseptr;
            if (!nvmem::reserve(maximum_address_range, nvmem::ReadWrite, baseptr))
                return false;

            const u32 page_size = nvmem::get_page_size();

            m_baseptr          = (u8*)baseptr;
            m_item_cap         = 0;
            m_item_size        = (item_size + (item_align - 1)) & ~(item_align - 1);
            m_item_count       = initial_item_count;
            u32 const page_max = (m_item_size * maximum_item_count) / page_size;

            u32 page_com = s_number_of_pages(m_item_size, initial_item_count, page_size);
            if (page_com > page_max)
            {
                page_com = page_max;
            }

            if (page_com > 0)
            {
                if (!nvmem::commit(m_baseptr, (u64)page_size * page_com))
                    return false;

                m_item_cap = (page_com * nvmem::get_page_size()) / m_item_size;
            }

            return true;
        }

        bool pool_t::exit()
        {
            const u32 page_size = nvmem::get_page_size();
            u32 const page_max = ((m_item_size * m_item_cap) + page_size - 1) / page_size;
            if (!nvmem::release(m_baseptr, page_max * page_size))
                return false;
            return true;
        }

        u32 pool_t::v_allocsize() const { return m_item_size; }

        void* pool_t::v_allocate()
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

        void pool_t::v_deallocate(void* ptr)
        {
            const u32 index = ptr2idx(ptr);
            *(u32*)ptr      = m_free_head;
            m_free_head     = index;
            m_item_count--;
        }

        void* pool_t::v_idx2ptr(u32 index) { return (void*)((u8*)m_baseptr + index * m_item_size); }
        u32   pool_t::v_ptr2idx(void const* ptr) const { return (u32)(((u8*)ptr - (u8*)m_baseptr) / m_item_size); }
    } // namespace nvmem
} // namespace ncore
