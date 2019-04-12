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

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
* @brief
*  Outputs to cout and fstream.
*/
class Output : public std::ostream
{
private:

    struct OutputBuffer : public std::streambuf
    {
        inline void addBuffer(std::streambuf* buf)
        {
            m_bufs.push_back(buf);
        }

        inline virtual char overflow(char c)
        {
            std::for_each(m_bufs.begin(), m_bufs.end(),
                std::bind2nd(std::mem_fun(&std::streambuf::sputc), c));
            return c;
        }

        std::vector<std::streambuf*> m_bufs;

    };

    OutputBuffer m_outputBuffer;
    std::ofstream m_file;

public:

    inline Output(const char* filename)
        : std::ostream(nullptr)
        , m_file(filename, std::ios::out)
    {
        std::ostream::rdbuf(&m_outputBuffer);
        m_outputBuffer.addBuffer(m_file.rdbuf());
        m_outputBuffer.addBuffer(std::cout.rdbuf());
    }
};

