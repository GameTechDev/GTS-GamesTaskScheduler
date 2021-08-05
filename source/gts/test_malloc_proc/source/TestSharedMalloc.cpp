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
#include "TestSharedMalloc.h"

#include <string>
#include <cstdlib>
#include <vector>
#include <thread>

#include "gts/malloc/GtsMallocRedirect.h"

static std::string g_string = "floss";

void doTest()
{
    size_t threadCount = std::thread::hardware_concurrency();
    std::vector<std::thread*> threads(threadCount);
    for (size_t ii = 0; ii < threadCount; ++ii)
    {
        threads[ii] = new std::thread([]{

            std::vector<void*> blocks;
            for (size_t iBlock = 0; iBlock < 1 << 12; ++iBlock)
            {
                blocks.push_back(malloc(8));
            }

            for (size_t iBlock = 0; iBlock < 1 << 12; ++iBlock)
            {
                free(blocks[iBlock]);
            }
        });
    }

    for (size_t ii = 0; ii < threadCount; ++ii)
    {
        threads[ii]->join();
        delete threads[ii];
    }
}

int main()
{
    // Load the dll.
    gts_malloc_redirect();

    doTest();
    return 0;
}


