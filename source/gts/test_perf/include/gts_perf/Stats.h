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

#include <vector>

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
* @brief
*  Basic statistics tracker for a data set.
*/
class Stats
{
public:

    Stats(size_t iterations)
    {
        dataSet.reserve(iterations);
    }

    void clear()
    {
        dataSet.clear();
    }

    void addDataPoint(double data)
    {
        dataSet.push_back(data);
    }

    double mean() const
    {
        double sum = 0;
        for (double dataPoint : dataSet)
        {
            sum += dataPoint;
        }

        return sum / dataSet.size();
    }

    double min() const
    {
        double minVal = DBL_MAX;
        for (double dataPoint : dataSet)
        {
            if (dataPoint < minVal)
            {
                minVal = dataPoint;
            }
        }

        return minVal;
    }

    double max() const
    {
        double maxVal = DBL_MIN;
        for (double dataPoint : dataSet)
        {
            if (dataPoint > maxVal)
            {
                maxVal = dataPoint;
            }
        }

        return maxVal;
    }

    double standardDeviation() const
    {
        double ave = mean();
        double variance = 0;

        for (double dataPoint : dataSet)
        {
            double delta = dataPoint - ave;
            variance += delta * delta;
        }

        return (1.0 / dataSet.size()) * sqrt(variance);
    }

private:

    // Store time as double, since write back with double colors results.
    std::vector<double> dataSet;
};
