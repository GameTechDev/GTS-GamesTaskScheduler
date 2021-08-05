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

#include "gts/platform/Assert.h"

namespace gts {

/**
 * @addtogroup Containers
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An intrusive doubly-linked list.
 */
class IntrusiveDList
{
public:

    //! An intrusive Node.
    struct Node
    {
        //! The previous node in the list.
        Node* pPrev = nullptr;

        //! The next node in the list.
        Node* pNext = nullptr;
    };

    //--------------------------------------------------------------------------
    Node const* front() const
    {
        return m_pHead;
    }

    //--------------------------------------------------------------------------
    Node const* back() const
    {
        return m_pTail;
    }

    //--------------------------------------------------------------------------
    Node* front()
    {
        return m_pHead;
    }

    //--------------------------------------------------------------------------
    Node* back()
    {
        return m_pTail;
    }

    //--------------------------------------------------------------------------
    Node* popFront()
    {
        if (!m_pHead)
        {
            return nullptr;
        }
        Node* pNode = m_pHead;
        remove(pNode);
        return pNode;
    }

    //--------------------------------------------------------------------------
    void pushFront(Node* pNode)
    {
        GTS_ASSERT(!containes(pNode));

        if (!m_pHead)
        {
            m_pHead = pNode;
            m_pTail = pNode;
        }
        else
        {
            m_pHead->pPrev = pNode;
            pNode->pNext = m_pHead;
            m_pHead = pNode;
        }

        GTS_ASSERT(size() == 0 ? m_pHead == m_pTail : true);
    }

    //--------------------------------------------------------------------------
    void pushBack(Node* pNode)
    {
        GTS_ASSERT(!containes(pNode));

        if (!m_pTail)
        {
            m_pHead = pNode;
            m_pTail = pNode;
        }
        else
        {
            m_pTail->pNext = pNode;
            pNode->pPrev = m_pTail;
            m_pTail = pNode;
        }

        GTS_ASSERT(size() == 0 ? m_pHead == m_pTail : true);
    }

    //--------------------------------------------------------------------------
    void remove(Node* pNode)
    {
        GTS_ASSERT(containes(pNode));

        if (!pNode->pPrev)
        {
            m_pHead = pNode->pNext;
        }
        else
        {
            pNode->pPrev->pNext = pNode->pNext;
        }

        if (!pNode->pNext)
        {
            m_pTail = pNode->pPrev;
        }
        else
        {
            pNode->pNext->pPrev = pNode->pPrev;
        }

        pNode->pPrev = nullptr;
        pNode->pNext = nullptr;

        GTS_ASSERT(size() == 0 ? m_pHead == m_pTail : true);
    }

    //--------------------------------------------------------------------------
    void clear()
    {
        m_pHead = nullptr;
        m_pTail = nullptr;
    }

    //--------------------------------------------------------------------------
    size_t size() const
    {
        uint32_t count = 0;
        Node* pNode = m_pHead;
        while (pNode)
        {
            count++;
            pNode = pNode->pNext;
        }

        uint32_t rcount = 0;
        pNode = m_pTail;
        while (pNode)
        {
            rcount++;
            pNode = pNode->pPrev;
        }

        GTS_ASSERT(count == rcount);

        return count;
    }

    //--------------------------------------------------------------------------
    bool empty() const
    {
        return m_pHead == nullptr;
    }

    //--------------------------------------------------------------------------
    bool containes(Node* pNode) const
    {
        Node* pCurr = m_pHead;
        while (pNode)
        {
            if (pCurr == pNode)
            {
                return true;
            }
            pNode = pNode->pNext;
        }
        return false;
    }

private:

    Node* m_pHead = nullptr;
    Node* m_pTail = nullptr;
};

/** @} */ // end of Containers

} // namespace gts
