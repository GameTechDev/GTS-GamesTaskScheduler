/*******************************************************************************
* Copyright 2019 Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files(the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
* 
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
******************************************************************************/
namespace internal {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// GuardBase:

//------------------------------------------------------------------------------
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
GuardBase<T, TAccessorSharedMutex, TGrowSharedMutex>::GuardBase(GuardBase&& other)
    : m_pItem(std::move(other.m_pItem))
    , m_pAccessorMutex(std::move(other.m_pAccessorMutex))
    , m_pGrowMutex(std::move(other.m_pGrowMutex))
{
    other.m_pAccessorMutex = nullptr;
    other.m_pGrowMutex     = nullptr;
}

//------------------------------------------------------------------------------
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
GuardBase<T, TAccessorSharedMutex, TGrowSharedMutex>&
GuardBase<T, TAccessorSharedMutex, TGrowSharedMutex>::operator=(GuardBase&& other)
{
    if(this != &other)
    {
        m_pItem                = std::move(other.m_pItem);
        m_pAccessorMutex       = std::move(other.m_pAccessorMutex);
        m_pGrowMutex           = std::move(other.m_pGrowMutex);

        other.m_pAccessorMutex = nullptr;
        other.m_pGrowMutex     = nullptr;
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
GuardBase<T, TAccessorSharedMutex, TGrowSharedMutex>::GuardBase(T* pItem, TAccessorSharedMutex* pAccessorMutex, TGrowSharedMutex* pGrowMutex)
    : m_pItem(pItem)
    , m_pAccessorMutex(pAccessorMutex)
    , m_pGrowMutex(pGrowMutex)
{}

//------------------------------------------------------------------------------
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
T const& GuardBase<T, TAccessorSharedMutex, TGrowSharedMutex>::get() const
{
    GTS_ASSERT(isValid());
    return *m_pItem;
}

//------------------------------------------------------------------------------
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
bool GuardBase<T, TAccessorSharedMutex, TGrowSharedMutex>::isValid() const
{
    return m_pItem != nullptr;
}

} // namespace internal

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// ReadGuard:

//------------------------------------------------------------------------------
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
ReadGuard<T, TAccessorSharedMutex, TGrowSharedMutex>::ReadGuard(T* pItem, TAccessorSharedMutex* pAccessorMutex, TGrowSharedMutex* pGrowMutex)
    : guard_base_type(pItem, pAccessorMutex, pGrowMutex)
{}

//------------------------------------------------------------------------------
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
ReadGuard<T, TAccessorSharedMutex, TGrowSharedMutex>::~ReadGuard()
{
    if(guard_base_type::m_pAccessorMutex)
    {
        guard_base_type::m_pAccessorMutex->unlock_shared();
    }

    if(guard_base_type::m_pGrowMutex)
    {
        guard_base_type::m_pGrowMutex->unlock_shared();
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// WriteGuard:

//------------------------------------------------------------------------------
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
WriteGuard<T, TAccessorSharedMutex, TGrowSharedMutex>::WriteGuard(T* pItem, TAccessorSharedMutex* pAccessorMutex, TGrowSharedMutex* pGrowMutex)
    : guard_base_type(pItem, pAccessorMutex, pGrowMutex)
{}

//------------------------------------------------------------------------------
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
WriteGuard<T, TAccessorSharedMutex, TGrowSharedMutex>& 
WriteGuard<T, TAccessorSharedMutex, TGrowSharedMutex>::operator=(T const& val)
{
    *guard_base_type::m_pItem = val;
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
WriteGuard<T, TAccessorSharedMutex, TGrowSharedMutex>&
WriteGuard<T, TAccessorSharedMutex, TGrowSharedMutex>::operator=(T&& val)
{
    *guard_base_type::m_pItem = std::move(val);
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
WriteGuard<T, TAccessorSharedMutex, TGrowSharedMutex>::~WriteGuard()
    {
        if(guard_base_type::m_pAccessorMutex)
        {
            guard_base_type::m_pAccessorMutex->unlock();
        }

        if(guard_base_type::m_pGrowMutex)
        {
            guard_base_type::m_pGrowMutex->unlock_shared();
        }
    }

//------------------------------------------------------------------------------
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
T& WriteGuard<T, TAccessorSharedMutex, TGrowSharedMutex>::get()
{
    GTS_ASSERT(guard_base_type::isValid());
    return *guard_base_type::m_pItem;
}
