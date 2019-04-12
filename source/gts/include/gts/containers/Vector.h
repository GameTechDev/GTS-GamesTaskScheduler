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
#pragma once

#ifndef GTS_USE_CUSTOM_VECTOR
#include <vector>
#endif

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A wrapper class for a stl-like vector. Uses the STL by default. Define
 *  GTS_USE_CUSTOM_VECTOR to add your own backend, or replace everything
 *  completely as long as this interface contract remains.
 */
template<typename T, typename TAlloc = std::allocator<T>>
class Vector
{
public:

#ifndef GTS_USE_CUSTOM_VECTOR
    using iterator               = typename std::vector<T, TAlloc>::iterator;
    using const_iterator         = typename std::vector<T, TAlloc>::const_iterator;
    using reverse_iterator       = typename std::vector<T, TAlloc>::reverse_iterator;
    using const_reverse_iterator = typename std::vector<T, TAlloc>::const_reverse_iterator;
#else
    #error "Missing custom Vector iterator."
#endif

    Vector() = default;
    explicit Vector(size_t count, const TAlloc& alloc = TAlloc());
    Vector(size_t count, T const& fill, const TAlloc& alloc = TAlloc());

    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;

    iterator end();
    const_iterator end() const;
    const_iterator cend() const;

    reverse_iterator rbegin();
    const_reverse_iterator rbegin() const;
    const_reverse_iterator crbegin() const;

    reverse_iterator rend();
    const_reverse_iterator rend() const;
    const_reverse_iterator crend() const;

    T& operator[](size_t pos);
    T const& operator[](size_t pos) const;

    T& back();
    T const& back() const;

    T& front();
    T const& front() const;
    
    void push_back(T const& val);
    void push_back(T&& val);
    void pop_back();
    void resize(size_t count);
    void resize(size_t count, T const& val);
    void reserve(size_t count);
    void clear();
    void shrink_to_fit();
    T* data();
    T const* data() const;

    bool empty() const;
    size_t size() const;

private:

#ifndef GTS_USE_CUSTOM_VECTOR
    std::vector<T, TAlloc> m_vec;
#else
    #error "Missing custom Vector type."
#endif
};

#ifndef GTS_USE_CUSTOM_VECTOR
    #include "VectorImpl_STL.inl"
#else
    #error "Missing custom vector implementation."
#endif


} // namespace gts
