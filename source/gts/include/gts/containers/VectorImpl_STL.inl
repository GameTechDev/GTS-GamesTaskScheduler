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

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
Vector<T, TAlloc>::Vector(size_t count, const TAlloc& alloc)
    : m_vec(count, T(), alloc)
{
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
Vector<T, TAlloc>::Vector(size_t count, T const& fill, const TAlloc& alloc)
    : m_vec(count, fill, alloc)
{
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
typename Vector<T, TAlloc>::iterator Vector<T, TAlloc>::begin()
{
    return m_vec.begin();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
typename Vector<T, TAlloc>::const_iterator Vector<T, TAlloc>::begin() const
{
    return m_vec.begin();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
typename Vector<T, TAlloc>::const_iterator Vector<T, TAlloc>::cbegin() const
{
    return m_vec.cbegin();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
typename Vector<T, TAlloc>::iterator Vector<T, TAlloc>::end()
{
    return m_vec.end();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
typename Vector<T, TAlloc>::const_iterator Vector<T, TAlloc>::end() const
{
    return m_vec.end();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
typename Vector<T, TAlloc>::const_iterator Vector<T, TAlloc>::cend() const
{
    return m_vec.cend();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
typename Vector<T, TAlloc>::reverse_iterator Vector<T, TAlloc>::rbegin()
{
    return m_vec.rbegin();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
typename Vector<T, TAlloc>::const_reverse_iterator Vector<T, TAlloc>::rbegin() const
{
    return m_vec.rbegin();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
typename Vector<T, TAlloc>::const_reverse_iterator Vector<T, TAlloc>::crbegin() const
{
    return m_vec.crbegin();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
typename Vector<T, TAlloc>::reverse_iterator Vector<T, TAlloc>::rend()
{
    return m_vec.rend();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
typename Vector<T, TAlloc>::const_reverse_iterator Vector<T, TAlloc>::rend() const
{
    return m_vec.rend();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
typename Vector<T, TAlloc>::const_reverse_iterator Vector<T, TAlloc>::crend() const
{
    return m_vec.crend();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
T& Vector<T, TAlloc>::operator[](size_t pos)
{
    return m_vec[pos];
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
T const& Vector<T, TAlloc>::operator[](size_t pos) const
{
    return m_vec[pos];
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
T& Vector<T, TAlloc>::back()
{
    return m_vec.back();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
T const& Vector<T, TAlloc>::back() const
{
    return m_vec.back();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
T& Vector<T, TAlloc>::front()
{
    return m_vec.front();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
T const& Vector<T, TAlloc>::front() const
{
    return m_vec.front();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
void Vector<T, TAlloc>::push_back(T const& val)
{
    m_vec.push_back(val);
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
void Vector<T, TAlloc>::push_back(T&& val)
{
    m_vec.push_back(std::move(val));
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
void Vector<T, TAlloc>::pop_back()
{
    m_vec.pop_back();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
void Vector<T, TAlloc>::resize(size_t count)
{
    m_vec.resize(count);
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
void Vector<T, TAlloc>::resize(size_t count, T const& val)
{
    m_vec.resize(count, val);
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
void Vector<T, TAlloc>::reserve(size_t count)
{
    m_vec.reserve(count);
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
void Vector<T, TAlloc>::clear()
{
    m_vec.clear();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
void Vector<T, TAlloc>::shrink_to_fit()
{
    m_vec.shrink_to_fit();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
T* Vector<T, TAlloc>::data()
{
    return m_vec.data();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
T const* Vector<T, TAlloc>::data() const
{
    return m_vec.data();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
bool Vector<T, TAlloc>::empty() const
{
    return m_vec.empty();
}

//------------------------------------------------------------------------------
template<typename T, typename TAlloc>
size_t Vector<T, TAlloc>::size() const
{
    return m_vec.size();
}
