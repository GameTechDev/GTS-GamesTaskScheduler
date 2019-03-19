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
#include "gts/analysis/ConcurrentLogger.h"

#include <vector>
#include <map>
#include <fstream>
#include <string>
#include <iomanip>

namespace gts {
namespace analysis {

//------------------------------------------------------------------------------
void printLogToTextFile(
    char const* filename,
    ConcurrentLoggerEvent const* pEvents,
    size_t const eventCount,
    Tag::Type tagsToInclude)
{
    size_t const paramLen = 6;
    size_t maxMsgLen = 0;

    size_t nextIdx = 0;
    std::map<size_t, size_t> indexByThreadId;
    for (size_t ii = 0; ii < eventCount; ++ii)
    {
        if (indexByThreadId.find(pEvents[ii].tid) == indexByThreadId.end())
        {
            indexByThreadId[pEvents[ii].tid] = nextIdx++;
        }

        size_t msgLen = strlen(pEvents[ii].msg);
        if (msgLen > maxMsgLen)
        {
            maxMsgLen = msgLen;
        }
    }

    std::fstream out(filename, std::ios::out);
    GTS_ASSERT(out.good());

    for (auto iter : indexByThreadId)
    {
        out << std::setw(maxMsgLen + paramLen * 2) << std::setfill(' ') << iter.first << " |";
    }

    out << "\n";

    for (size_t ii = 0; ii < eventCount; ++ii)
    {
        ConcurrentLoggerEvent const& e = pEvents[ii];

        if ((tagsToInclude != 0 && (e.tag & tagsToInclude)) == 0)
        {
            break; // filter out
        }

        size_t index = indexByThreadId[e.tid];
        for (size_t jj = 0; jj < index; ++jj)
        {
            out << std::setw(maxMsgLen + paramLen * 2) << std::setfill(' ') << " " << " |";
        }

        out << std::left << std::setw(maxMsgLen) << std::setfill(' ') << e.msg;
        out << std::setw(paramLen) << std::setfill(' ') << e.param1 % 100000;
        out << std::setw(paramLen) << std::setfill(' ') << e.param2 % 100000;
        out << " |";

        for (size_t jj = index + 1; jj < indexByThreadId.size(); ++jj)
        {
            out << std::setw(maxMsgLen + paramLen * 2) << std::setfill(' ') << " " << " |";
        }

        out << "\n";
    }
}

//------------------------------------------------------------------------------
void printLogToCsvFile(
    char const* filename,
    ConcurrentLoggerEvent const* pEvents,
    size_t const eventCount,
    Tag::Type tagsToInclude)
{
    size_t nextIdx = 0;
    std::map<size_t, size_t> indexByThreadId;
    for (size_t ii = 0; ii < eventCount; ++ii)
    {
        if (indexByThreadId.find(pEvents[ii].tid) == indexByThreadId.end())
        {
            indexByThreadId[pEvents[ii].tid] = nextIdx++;
        }
    }

    std::fstream out(filename, std::ios::out);
    GTS_ASSERT(out.good());

    for (auto iter : indexByThreadId)
    {
        out << iter.first << ",";
    }

    out << "\n";

    for (size_t ii = 0; ii < eventCount; ++ii)
    {
        ConcurrentLoggerEvent const& e = pEvents[ii];

        if ((tagsToInclude != 0 && (e.tag & tagsToInclude)) == 0)
        {
            break; // filter out
        }

        size_t index = indexByThreadId[e.tid];
        for (size_t jj = 0; jj < index; ++jj)
        {
            out << " ,";
        }

        out << e.msg << '|' << e.param1 << '|' << e.param2;
        out << "\n";
    }
}

} // namespace analysis
} // namespace gts
