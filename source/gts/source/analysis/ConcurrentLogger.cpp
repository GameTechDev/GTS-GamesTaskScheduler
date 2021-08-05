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

#include <cstring>
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
    std::map<std::thread::id, std::string> const& threadNamesMap,
    ConcurrentLoggerEvent const* pEvents,
    size_t const eventCount)
{
    size_t const paramLen = 6;
    size_t maxMsgLen = 0;

    size_t nextIdx = 0;
    std::map<std::thread::id, size_t> indexByThreadId;
    for (size_t ii = 0; ii < eventCount; ++ii)
    {
        ConcurrentLoggerEvent const& e = pEvents[ii];

        if (indexByThreadId.find(e.tid) == indexByThreadId.end())
        {
            indexByThreadId[e.tid] = nextIdx++;
        }

        size_t msgLen = 0;
        if (e.isMessageBuffer)
        {
            msgLen = strlen(e.message.buff);
        }
        else
        {
            msgLen = strlen(e.message.msgAndParams.msg);
        }

        if (msgLen > maxMsgLen)
        {
            maxMsgLen = msgLen;
        }
    }

    maxMsgLen += 1;

    std::fstream out(filename, std::ios::out);
    assert(out.good());

    std::map<size_t, std::pair<std::thread::id, size_t>> indexSortedThreads;
    for (auto iter : indexByThreadId)
    {
        indexSortedThreads[iter.second] = iter;
    }
    for (auto iter : indexSortedThreads)
    {
        constexpr size_t BUFF_SIZE = 256;
        char buff[BUFF_SIZE];
        auto nameIter =  threadNamesMap.find(iter.second.first);
        size_t tid = std::hash<std::thread::id>{}(iter.second.first);
        if(nameIter != threadNamesMap.end())
        {
            const char* name = nameIter->second.c_str();
            snprintf(buff, BUFF_SIZE, "%zx:%s", tid, name);
        }
        else
        {
            snprintf(buff, BUFF_SIZE, "%zx", tid);
        }
        out << std::setw(maxMsgLen + paramLen * 3) << std::setfill(' ') << buff << " |";
    }

    out << "\n";

    for (size_t ii = 0; ii < eventCount; ++ii)
    {
        ConcurrentLoggerEvent const& e = pEvents[ii];

        size_t index = indexByThreadId[e.tid];
        for (size_t jj = 0; jj < index; ++jj)
        {
            out << std::setw(maxMsgLen + paramLen * 3) << std::setfill(' ') << " " << " |";
        }

        if (e.isMessageBuffer)
        {
            out << std::left << std::setw(maxMsgLen + paramLen * 3) << std::setfill(' ') << e.message.buff;
        }
        else
        {
            out << std::left << std::setw(maxMsgLen) << std::setfill(' ') << e.message.msgAndParams.msg;
            for (uint8_t iParam = 0; iParam < e.message.msgAndParams.numParams; ++iParam)
            {
                out << std::setw(paramLen) << std::setfill(' ') << e.message.msgAndParams.params[iParam] % 100000;
            }
        }

        out << " |";

        for (size_t jj = index + 1; jj < indexByThreadId.size(); ++jj)
        {
            out << std::setw(maxMsgLen + paramLen * 3) << std::setfill(' ') << " " << " |";
        }

        out << "\n";
    }
    out.flush();
}

//------------------------------------------------------------------------------
void printLogToCsvFile(
    char const* filename,
    std::map<std::thread::id, std::string> const& threadNamesMap,
    ConcurrentLoggerEvent const* pEvents,
    size_t const eventCount)
{
    size_t nextIdx = 0;
    std::map<std::thread::id, size_t> indexByThreadId;
    for (size_t ii = 0; ii < eventCount; ++ii)
    {
        if (indexByThreadId.find(pEvents[ii].tid) == indexByThreadId.end())
        {
            indexByThreadId[pEvents[ii].tid] = nextIdx++;
        }
    }

    std::fstream out(filename, std::ios::out);
    assert(out.good());

    for (auto iter : indexByThreadId)
    {
        out << iter.first << "|" << threadNamesMap.find(iter.first)->second << ",";
    }

    out << "\n";

    for (size_t ii = 0; ii < eventCount; ++ii)
    {
        ConcurrentLoggerEvent const& e = pEvents[ii];

        size_t index = indexByThreadId[e.tid];
        for (size_t jj = 0; jj < index; ++jj)
        {
            out << " ,";
        }

        if (e.isMessageBuffer)
        {
            out << e.message.buff;
        }
        else
        {
            out << e.message.msgAndParams.msg;
            for (uint8_t iParam = 0; iParam < e.message.msgAndParams.numParams; ++iParam)
            {
                out << '|' << e.message.msgAndParams.params[iParam];
            }
        }

        out << "\n";
    }
}

} // namespace analysis
} // namespace gts
