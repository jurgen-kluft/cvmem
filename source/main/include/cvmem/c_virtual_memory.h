#ifndef __C_VMEM_VIRTUAL_MEMORY_INTERFACE_H__
#define __C_VMEM_VIRTUAL_MEMORY_INTERFACE_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

namespace ncore
{
    class vmem_t
    {
    public:
        virtual bool initialize(u32 pagesize=0) = 0;

        virtual bool reserve(u64 address_range, u32& page_size, u32 attributes, void*& baseptr) = 0;
        virtual bool release(void* baseptr, u64 address_range)                                  = 0;

        virtual bool commit(void* address, u32 page_size, u32 page_count)   = 0;
        virtual bool decommit(void* address, u32 page_size, u32 page_count) = 0;
    };

    extern vmem_t* vmem;

}; // namespace ncore

#endif /// __C_VMEM_VIRTUAL_MEMORY_INTERFACE_H__