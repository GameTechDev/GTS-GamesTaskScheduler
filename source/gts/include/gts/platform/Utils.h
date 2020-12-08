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

#include <cstring>
#include <utility>
#include <type_traits>

#include "gts/platform/Machine.h"
#include "gts/platform/Assert.h"

namespace gts {

/** 
* @addtogroup Platform
* @{
*/

/** 
 * @defgroup Utilities Utilities
 *  A set of utility functions.
 * @{
 */

//------------------------------------------------------------------------------
/**
 * @brief Calculates the max of \a lhs and \a rhs.
 * @return The maximum value.
 */
template<typename T>
constexpr T gtsMax(T const& lhs, T const& rhs)
{
    return lhs > rhs ? lhs : rhs;
}

//------------------------------------------------------------------------------
/**
 * @brief Calculates the min of \a lhs and rhs.
 * @return The minimum value.
 */
template<typename T>
constexpr T gtsMin(T const& lhs, T const& rhs)
{
    return lhs < rhs ? lhs : rhs;
}

//------------------------------------------------------------------------------
/**
 * @brief The root template for numeric limits.
 */
template<typename T>
struct numericLimits {};

//------------------------------------------------------------------------------
/**
 * @brief The numeric limits for uint32_t.
 */
template<>
struct numericLimits<uint32_t>
{ 
    static constexpr uint32_t min() { return 0; };
    static constexpr uint32_t max() { return UINT32_MAX; };
};

//------------------------------------------------------------------------------
/**
 * @brief The numeric limits for uint64_t.
 */
template<>
struct numericLimits<uint64_t>
{
    static constexpr uint64_t min() { return 0; };
    static constexpr uint64_t max() { return UINT64_MAX; };
};

//------------------------------------------------------------------------------
/**
 * @brief Checks if \a value is a power of 2.
 * @returns True if \a value is a power of 2.
 */
template<typename T>
constexpr bool isPow2(T value)
{
    static_assert(std::is_integral<T>::value, "T must be an integral type.");
    return value && !(value & (value - 1));
}

//------------------------------------------------------------------------------
/**
 * @brief A power of 2 type trait.
 */
template<typename T, T val>
struct IsPow2
{
    enum { value = isPow2(val) };
};

//------------------------------------------------------------------------------
/**
 * @brief Calculates the next 16-bit power of 2 if \a value is not a power of 2.
 * @return The next power of 2 after \a value or \a value.
 */
GTS_INLINE constexpr uint16_t nextPow2(uint16_t value)
{
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    return ++value;
}

//------------------------------------------------------------------------------
/**
 * @brief Calculates the next 32-bit power of 2 if \a value is not a power of 2.
 * @return The next power of 2 after \a value or \a value.
 */
GTS_INLINE constexpr uint32_t nextPow2(uint32_t value)
{
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return ++value;
}

//------------------------------------------------------------------------------
/**
 * @brief Calculates the next 64-bit power of 2 if \a value is not a power of 2.
 * @return The next power of 2 after \a value or \a value.
 */
GTS_INLINE constexpr uint64_t nextPow2(uint64_t value)
{
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return ++value;
}

//------------------------------------------------------------------------------
/**
 * @brief Calculates log2 a integer.
 * @return Log2 of \a integral.
 */
template<typename TInt>
GTS_INLINE TInt log2i(TInt const& integral)
{
    static_assert(std::is_integral<TInt>::value, "TInt must be an integral type.");
    return GTS_MSB_SCAN((uint32_t)integral);
}

//------------------------------------------------------------------------------
/**
 * @brief Specialization to calculate the log2 a signed 64-bit integer.
 * @return Log2 of \a integral.
 */
template<>
GTS_INLINE int64_t log2i(int64_t const& integral)
{
    return GTS_MSB_SCAN64(integral);
}

//------------------------------------------------------------------------------
/**
 * @brief Specialization to calculate the log2 a unsigned 64-bit integer.
 * @return Log2 of \a integral.
 */
template<>
GTS_INLINE uint64_t log2i(uint64_t const& integral)
{
    return GTS_MSB_SCAN64(integral);
}

//------------------------------------------------------------------------------
/**
 * @brief Generates a pseudo random number using xor shift algorithm on 'state'.
 * @return A pseudo random number.
 */
GTS_INLINE uint32_t fastRand(uint32_t& state)
{
    GTS_ASSERT(state != 0);
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

//------------------------------------------------------------------------------
/**
 * @brief Checks if \a ptr is aligned to the boundary \a alignmentPow2.
 * @return True if aligned, false otherwise.
 */
GTS_INLINE bool isAligned(void* ptr, size_t alignmentPow2)
{
    GTS_ASSERT(isPow2(alignmentPow2));
    return ((uintptr_t)ptr & (alignmentPow2 - 1)) == 0;
}

//------------------------------------------------------------------------------
/**
 * @brief Aligns \a ptr down to the next \a alignment boundary.
 * @return An aligned address.
 */
GTS_INLINE uintptr_t alignDownTo(uintptr_t ptr, size_t alignment)
{
    uintptr_t mask = alignment - 1;
    if(isPow2(alignment))
    {
        return ptr & ~mask;
    }
    else
    {
        return (ptr / alignment) * alignment;
    }
}

//------------------------------------------------------------------------------
/**
 * @brief Aligns \a ptr up to the next \a alignment boundary.
 * @return An aligned address.
 */
GTS_INLINE uintptr_t alignUpTo(uintptr_t ptr, size_t alignment)
{
    uintptr_t mask = alignment - 1;
    if(isPow2(alignment))
    {
        return (ptr + mask) & ~mask;
    }
    else
    {
        return ((ptr + mask) * alignment) / alignment;
    }
}

//! The type used to identify an object.
using IdType = uint32_t;

//! The sub type stored in IdType.
using SubIdType = uint16_t;

//! The value of an unknown IdType.
constexpr IdType UNKNOWN_UID = UINT32_MAX;

//! The value of an unknown SubIdType.
constexpr SubIdType UNKNOWN_SUBID  = UINT16_MAX;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An ID for owned objects. It indicates the owner's ID, an ID local to the
 *   owner, and an overall unique ID.
 */
class OwnedId
{
public:

    GTS_INLINE OwnedId() : m_uid(UNKNOWN_UID)
    {}

    GTS_INLINE OwnedId(SubIdType ownerId, SubIdType localId)
        : m_uid((ownerId << SHIFT) | localId)
    {}

    /**
     * @return The ID of the object local to its owner.
     */
    GTS_INLINE SubIdType localId() const
    {
        return (SubIdType)(m_uid & LOCAL);
    }

    /**
     * @return The unique ID of the owner object.
     */
    GTS_INLINE SubIdType ownerId() const
    {
        return (SubIdType)((m_uid & OWNER) >> SHIFT);
    }

    /**
     * @return The unique ID object.
     */
    GTS_INLINE IdType uid() const
    {
        return m_uid;
    }

    /**
     * @return True if equal.
     */
    GTS_INLINE bool operator==(OwnedId const& rhs) const
    {
        return m_uid == rhs.m_uid;
    }

    /**
     * @return True if not equal.
     */
    GTS_INLINE bool operator!=(OwnedId const& rhs) const
    {
        return m_uid != rhs.m_uid;
    }

private:

    enum { OWNER = 0xFFFF0000, LOCAL = 0x0000FFFF, SHIFT = sizeof(SubIdType) * 8 };
    IdType m_uid;
};

//------------------------------------------------------------------------------
/**
 * @return The murmur hash of a 32-bit \a key.
 */
GTS_INLINE uint32_t murmur_hash3(uint32_t key)
{
    // https://github.com/aappleby/smhasher

    key ^= key >> 16;
    key *= 0x85ebca6b;
    key ^= key >> 13;
    key *= 0xc2b2ae35;
    key ^= key >> 16;
    return key;
}

//------------------------------------------------------------------------------
/**
 * @return The murmur hash of a 64-bit \a key.
 */
GTS_INLINE uint64_t murmur_hash3(uint64_t key)
{
    // https://github.com/aappleby/smhasher

    key ^= key >> 33;
    key *= 0xff51afd7ed558ccd;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53;
    key ^= key >> 33;
    return key;
}

//------------------------------------------------------------------------------
/**
 * @return The murmur hash of a string.
 */
GTS_INLINE uint32_t murmur_hash3(const void * key, uint32_t len, uint32_t seed = 12345)
{
    // https://github.com/aappleby/smhasher

    // 'm' and 'r' are mixing constants generated off-line.
    // They're not really 'magic', they just happen to work well.

    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    // Initialize the hash to a 'random' value

    uint32_t h = seed ^ len;

    // Mix 4 bytes at a time into the hash

    const unsigned char * data = (const unsigned char *)key;

    while (len >= 4)
    {
        uint32_t k = *(uint32_t*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array

    switch (len)
    {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0];
        h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

//------------------------------------------------------------------------------
/**
 * @brief
 *  The base murmur hash functor.
 */
template<class TKey>
struct MurmurHash
{
    using hashed_value = size_t;

    GTS_INLINE hashed_value operator()(const TKey& key) const
    {
        typename std::make_unsigned<TKey>::type ukey = key;
        return (hashed_value)murmur_hash3(ukey);
    }
};

//------------------------------------------------------------------------------
/**
 * @brief
 *  The float specialized murmur hash functor.
 */
template<>
struct MurmurHash<float>
{
    using hashed_value = uint32_t;

    GTS_INLINE hashed_value operator()(float key) const
    {
        uint32_t ukey = (uint32_t)key;
        return murmur_hash3(ukey);
    }
};

//------------------------------------------------------------------------------
/**
 * @brief
 *  The double specialized murmur hash functor.
 */
template<>
struct MurmurHash<double>
{
    using hashed_value = uint64_t;

    GTS_INLINE hashed_value operator()(double key) const
    {
        uint64_t ukey = (uint64_t)key;
        return murmur_hash3(ukey);
    }
};

//------------------------------------------------------------------------------
/**
 * @brief
 *  The c-string specialized murmur hash functor.
 */
template<>
struct MurmurHash<char*>
{
    using hashed_value = uint64_t;

    GTS_INLINE hashed_value operator()(const char* key) const
    {
        size_t len = strlen(key);
        return murmur_hash3(key, (uint32_t)len);
    }
};

//------------------------------------------------------------------------------
/**
 * @brief
 *  The pointer specialized murmur hash functor.
 */
template<class TKey>
struct MurmurHash<TKey*>
{
    using hashed_value = size_t;

    GTS_INLINE hashed_value operator()(TKey* key) const
    {
        return murmur_hash3(reinterpret_cast<uintptr_t>(key));
    }
};

//------------------------------------------------------------------------------
/**
 * @brief
 *  A simple function invoker.
 */
template<typename TResult, typename TFunc, typename... TArgs>
GTS_INLINE TResult invoke(TFunc&& func, TArgs&&... args)
{
    return func(std::forward<TArgs>(args)...);
}

//------------------------------------------------------------------------------
/**
 * @brief
 *  Simple insertion sort implementation.
 */
template<typename Value, typename TCompareFunc>
GTS_INLINE void insertionSort(Value* values, size_t numValues, TCompareFunc fcnComp)
{
    for (size_t ii = 1; ii < numValues; ++ii)
    {
        Value v   = values[ii];
        size_t jj = 0;

        while (jj < ii && fcnComp(v, values[ii - jj - 1]))
        {
            values[ii - jj] = values[ii - jj - 1];
            ++jj;
        }

        values[ii - jj] = v;
        
    }
}

/** @} */ // end of Utilities
/** @} */ // end of Platform

} // namespace gts
