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
#include "macro_scheduler/ComputeResourceFactory.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// ComputeResourcePackage

//------------------------------------------------------------------------------
ComputeResourcePackage::~ComputeResourcePackage()
{
    for (uint32_t ii = 0; ii < computeResources.size(); ++ii)
    {
        alignedDelete(computeResources[computeResources.size() - ii - 1]);
    }
    for (uint32_t ii = 0; ii < microSchedulers.size(); ++ii)
    {
        alignedDelete(microSchedulers[microSchedulers.size() - ii - 1]);
    }
    for (uint32_t ii = 0; ii < workerPools.size(); ++ii)
    {
        alignedDelete(workerPools[workerPools.size() - ii - 1]);
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// MircoSchedulerComputeResourceFactory

//------------------------------------------------------------------------------
MircoSchedulerComputeResourceFactory::MircoSchedulerComputeResourceFactory(
    uint32_t numResources,
    uint32_t threadCount)
    : m_numResources(numResources)
    , m_threadCount(threadCount)
{}

//------------------------------------------------------------------------------
void MircoSchedulerComputeResourceFactory::buildComputeResources(
    ComputeResourcePackage& computeResourcePkg)
{
    Vector<WorkerPool*> workerPools;
    Vector<MicroScheduler*> microSchedulers;
    for (uint32_t ii = 0; ii < m_numResources; ++ii)
    {
        uint32_t threadCount = m_threadCount / m_numResources;
        WorkerPool* workerPool = alignedNew<WorkerPool, GTS_NO_SHARING_CACHE_LINE_SIZE>();
        workerPool->initialize(threadCount);
        computeResourcePkg.workerPools.push_back(workerPool);

        MicroScheduler* microScheduler = alignedNew<MicroScheduler, GTS_NO_SHARING_CACHE_LINE_SIZE>();
        microScheduler->initialize(workerPool);
        computeResourcePkg.microSchedulers.push_back(microScheduler);

        MicroScheduler_ComputeResource* microSchedulerCompResource = alignedNew<MicroScheduler_ComputeResource, GTS_NO_SHARING_CACHE_LINE_SIZE>(microScheduler, 0, threadCount);
        microSchedulerCompResource->setExecutionNormalizationFactor(ii + 1);
        computeResourcePkg.computeResources.push_back(microSchedulerCompResource);
    }

    for (uint32_t ii = 0; ii < m_numResources; ++ii)
    {
        for (uint32_t jj = 0; jj < m_numResources; ++jj)
        {
            if (ii != jj)
            {
                computeResourcePkg.microSchedulers[ii]->addExternalVictim(computeResourcePkg.microSchedulers[jj]);
            }
        }
    }
}
