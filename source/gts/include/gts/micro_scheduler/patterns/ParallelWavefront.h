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
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/Partitioners.h"
#include "gts/micro_scheduler/patterns/Range1d.h"

namespace gts {

/** 
 * @addtogroup MicroScheduler
 * @{
 */

/** 
 * @addtogroup ParallelPatterns
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template<typename TValRange, typename TDepRange, uint32_t DIMENSIONALITY>
class DependencyArray
{
    static_assert(DIMENSIONALITY <= 3 && DIMENSIONALITY != 1, "Unsupported dimensionality");
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template<typename TValRange, typename TDepRange>
class DependencyArray<TValRange, TDepRange, 2>
{
public:

    DependencyArray() = default;

    //--------------------------------------------------------------------------
    DependencyArray(Atomic<uint8_t>** m_ppDependencies, size_t xSize, size_t ySize)
        : m_ppDependencies(m_ppDependencies)
        , m_xSize(xSize)
        , m_ySize(ySize)
    {}

    //--------------------------------------------------------------------------
    template<typename TSplitter>
    void produceSplits(
        TSplitter const& splitter,
        TValRange const& valRange,
        TValRange* pReadyRanges,
        uint8_t& readyIdx)
    {
        // Build the dependencies range

        size_t xBegin = valRange.xRange().begin() / valRange.xRange().minSize();
        size_t xEnd   = valRange.xRange().end() / valRange.xRange().minSize();
        size_t yBegin = valRange.yRange().begin() / valRange.yRange().minSize();
        size_t yEnd   = valRange.yRange().end() / valRange.yRange().minSize();
        TDepRange depRange(xBegin, xEnd, 1, yBegin, yEnd, 1);
        depRange.resetInitialSize(m_xSize, SubRangeIndex::X);
        depRange.resetInitialSize(m_ySize, SubRangeIndex::Y);

        // Get any ready x-neighbors

        TValRange valNeighbor = valRange.xNeighbor();
        if(!valNeighbor.xRange().empty())
        {
            TDepRange const originalDepNeighbor = depRange.xNeighbor();
            TDepRange depNeighbor = originalDepNeighbor;

            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "produceSplits check x-neighbor X", depNeighbor.xRange().begin(), depNeighbor.xRange().end());
            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "produceSplits check x-neighbor Y", depNeighbor.yRange().begin(), depNeighbor.yRange().end());

            size_t x = depNeighbor.xRange().begin();
            size_t y = depNeighbor.yRange().begin();

            if(m_ppDependencies[x][y].load(memory_order::acquire) == 0)
            {
                GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "BOOM", x, y);
                GTS_ASSERT(m_ppDependencies[x][y].load(memory_order::acquire) != 0);
            }
            
            x = depNeighbor.xRange().begin();
            y = depNeighbor.yRange().begin();
            bool ready = m_ppDependencies[x][y].fetch_sub(1, memory_order::seq_cst) - 1 == 0;
            for (++y; y < depNeighbor.yRange().end(); ++y)
            {
                m_ppDependencies[x][y].fetch_sub(1, memory_order::seq_cst);
            }

            if (ready)
            {
                Vector<uint8_t> topCounts(depNeighbor.xRange().size());
                size_t topCountOffset = depNeighbor.xRange().begin();
                x = depNeighbor.xRange().begin();
                y = depNeighbor.yRange().begin();
                for (; x < depNeighbor.xRange().end(); ++x)
                {
                    topCounts[x - topCountOffset] =
                        m_ppDependencies[x][y].load(memory_order::acquire);
                }

                if (_splitToReady(
                    splitter,
                    topCounts,
                    topCountOffset,
                    depNeighbor,
                    valNeighbor,
                    SubRangeIndex::X))
                {
                    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "produceSplits got x-neighbor X", depNeighbor.xRange().begin(), depNeighbor.xRange().end());
                    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "produceSplits got x-neighbor Y", depNeighbor.yRange().begin(), depNeighbor.yRange().end());

                    pReadyRanges[readyIdx++] = valNeighbor;
                }
            }
            else
            {
                GTS_TRACE_ZONE_MARKER_P0(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "produceSplits x-neighbor not ready.");
            }
        }

        // Get any ready y-neighbors

        valNeighbor = valRange.yNeighbor();
        if(!valNeighbor.yRange().empty())
        {
            TDepRange const originalDepNeighbor = depRange.yNeighbor();
            TDepRange depNeighbor = originalDepNeighbor;

            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "produceSplits check y-neighbor X", depNeighbor.xRange().begin(), depNeighbor.xRange().end());
            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "produceSplits check y-neighbor Y", depNeighbor.yRange().begin(), depNeighbor.yRange().end());

            size_t x = depNeighbor.xRange().begin();
            size_t y = depNeighbor.yRange().begin();

            if(m_ppDependencies[x][y].load(memory_order::acquire) == 0)
            {
                GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "BOOM", x, y);
                GTS_ASSERT(m_ppDependencies[x][y].load(memory_order::acquire) != 0);
            }

            x = depNeighbor.xRange().begin();
            y = depNeighbor.yRange().begin();
            bool ready = m_ppDependencies[x][y].fetch_sub(1, memory_order::seq_cst) - 1 == 0;
            for (++x; x < depNeighbor.xRange().end(); ++x)
            {
                m_ppDependencies[x][y].fetch_sub(1, memory_order::seq_cst);
            }

            if (ready)
            {
                Vector<uint8_t> sideCounts(depNeighbor.yRange().size());
                size_t sideCountOffset = depNeighbor.yRange().begin();
                x = depNeighbor.xRange().begin();
                y = depNeighbor.yRange().begin();
                for (; y < depNeighbor.yRange().end(); ++y)
                {
                    sideCounts[y - sideCountOffset] =
                        m_ppDependencies[x][y].load(memory_order::acquire);
                }

                if (_splitToReady(
                    splitter,
                    sideCounts,
                    sideCountOffset,
                    depNeighbor,
                    valNeighbor,
                    SubRangeIndex::Y))
                {
                    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "produceSplits got y-neighbor X", depNeighbor.xRange().begin(), depNeighbor.xRange().end());
                    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "produceSplits got y-neighbor Y", depNeighbor.yRange().begin(), depNeighbor.yRange().end());

                    pReadyRanges[readyIdx++] = valNeighbor;
                }
            }
            else
            {
                GTS_TRACE_ZONE_MARKER_P0(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "produceSplits y-neighbor not ready.");
            }
        }
    }

private:

    //--------------------------------------------------------------------------
    template<typename TSplitter>
    bool _splitToReady(
        TSplitter const& splitter,
        Vector<uint8_t> const& counts,
        size_t countOffset,
        TDepRange& depRange,
        TValRange& valRange,
        SubRangeIndex::Type idx)
    {
        GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "_splitToReady");

        // If left-most column,
        if (depRange.subRange((SubRangeIndex::Type)((idx + 1) % 2)).begin() == 0)
        {
            if (counts[0] > 1)
            {
                // not ready.
                return false;
            }

            // range is ready.
            return true;
        }
        // Otherwise, do full ready check.
        else
        {
            if (counts[0] != 0)
            {
                // Not ready.
                return false;
            }

            bool sideAllZeros = true;
            for (size_t ii = depRange.subRange(idx).begin(); ii < depRange.subRange(idx).end(); ++ii)
            {
                if (counts[ii - countOffset] != 0)
                {
                    sideAllZeros = false;
                    break;
                }
            }

            // If the left row is not all zeros, split down to a
            // successor that we won.
            if (!sideAllZeros)
            {
                do
                {
                    if (!depRange.subRange(idx).isDivisible())
                    {
                        GTS_ASSERT(0);

                        // Can't divide the top any further.
                        return false;
                    }

                    typename TDepRange::split_result depSplits;
                    depRange.split(depSplits, splitter);

                    typename TValRange::split_result valSplits;
                    valRange.split(valSplits, splitter);


                    sideAllZeros = true;
                    for (size_t ii = depRange.subRange(idx).begin(); ii < depRange.subRange(idx).end(); ++ii)
                    {
                        if (counts[ii - countOffset] != 0)
                        {
                            sideAllZeros = false;
                            break;
                        }
                    }

                } while(!sideAllZeros);
            }
        }

        return true;
    }

private:

    Atomic<uint8_t>** m_ppDependencies = nullptr;
    size_t m_xSize = 0;
    size_t m_ySize = 0;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//template<typename TValRange, typename TDepRange>
//class DependencyArray<TValRange, TDepRange, 3>
//{
//public:

//    DependencyArray() = default;

//    //--------------------------------------------------------------------------
//    DependencyArray(Atomic<uint8_t>** pppDependencies, size_t xSize, size_t ySize, size_t zSize)
//        : pppDependencies(m_pppDependencies)
//        , m_xSize(xSize)
//        , m_ySize(ySize)
//        , m_zSize(zSize)
//    {}

//private:

//    Atomic<uint8_t>*** pppDependencies = nullptr;
//    size_t m_xSize = 0;
//    size_t m_ySize = 0;
//    size_t m_zSize = 0;
//};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template<uint32_t DIMENSIONALITY>
struct LaunchTask
{
    static_assert(DIMENSIONALITY != 1, "1D Wavefront not supported. Try ParallelScan.");
    static_assert(DIMENSIONALITY <= 3, "Unsupported dimensionality.");
    static_assert(DIMENSIONALITY != 3, "TODO: Add 3D support.");
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template<>
struct LaunchTask<2>
{
    enum { DIMENSIONALITY = 2 };

    template<
        typename TWavefrontTask,
        typename TFunc,
        typename TPartitioner,
        template<typename...> class TRange,
        class... TArgs
    >
    static GTS_INLINE void launch(
        MicroScheduler& microScheduler,
        uint32_t priority,
        TRange<TArgs...> const& range,
        TFunc wavefrontFunc,
        TPartitioner partitioner,
        void* pUserData)
    {
        GTS_ASSERT(microScheduler.isRunning());

        //
        // Build the array of dependency counters.

        using dep_arrary_type = DependencyArray<TRange<TArgs...>, TRange<size_t>, DIMENSIONALITY>;

        size_t xSize = range.xRange().size() / range.xRange().minSize() +
            (range.xRange().size() % range.xRange().minSize() != 0 ? 1 : 0);

        size_t ySize = range.yRange().size() / range.yRange().minSize() +
            (range.yRange().size() % range.yRange().minSize() != 0 ? 1 : 0);

        Atomic<uint8_t>** ppDependencies = new Atomic<uint8_t>*[xSize];
        for (size_t ii = 0; ii < xSize; ++ii)
        {
            ppDependencies[ii] = new Atomic<uint8_t>[xSize];
            for (size_t jj = 0; jj < ySize; ++jj)
            {
                ppDependencies[ii][jj].store((ii > 0) + (jj > 0), memory_order::relaxed);
            }
        }

        dep_arrary_type depArray(ppDependencies, xSize, ySize);

        //
        // Launch root task and wait.

        uint32_t workerCount = microScheduler.workerCount();
        partitioner.template initialize<TRange<TArgs...>>((uint16_t)workerCount);

        Task* pTask = microScheduler.template allocateTask<TWavefrontTask>(
            wavefrontFunc, pUserData, range, partitioner, priority, depArray);

        microScheduler.spawnTaskAndWait(pTask, priority);

        //
        // Delete the array of dependency counters.

        for (size_t ii = 0; ii < xSize; ++ii)
        {
            delete[] ppDependencies[ii];
        }
        delete[] ppDependencies;
    }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template<>
struct LaunchTask<3>
{
    enum { DIMENSIONALITY = 3 };

    //--------------------------------------------------------------------------
    template<
        typename TWavefrontTask,
        typename TFunc,
        typename TPartitioner,
        template<typename...> class TRange,
        class... TArgs
    >
    static GTS_INLINE void launch(
        MicroScheduler& microScheduler,
        uint32_t priority,
        TRange<TArgs...> const& range,
        TFunc wavefrontFunc,
        TPartitioner partitioner,
        void* pUserData)
    {
        using dep_arrary_type = DependencyArray<TRange<TArgs...>, TRange<size_t>, DIMENSIONALITY>;

        GTS_ASSERT(microScheduler.isRunning());

        //
        // Build the array of dependency counters.

        size_t xSize = range.xRange().size() / range.xRange().minSize() +
            range.xRange().size() % range.xRange().minSize() != 0 ? 1 : 0;

        size_t ySize = range.yRange().size() / range.yRange().minSize() +
            range.yRange().size() % range.yRange().minSize() != 0 ? 1 : 0;

        size_t zSize = range.zRange().size() / range.zRange().minSize() +
            range.zRange().size() % range.zRange().minSize() != 0 ? 1 : 0;

        Atomic<uint8_t>*** pppDependencies = new Atomic<uint8_t>**[xSize];
        for (size_t ii = 0; ii < xSize; ++ii)
        {
            pppDependencies[ii] = new Atomic<uint8_t>*[ySize];

            for (size_t jj = 0; jj < ySize; ++jj)
            {
                pppDependencies[ii][jj] = new Atomic<uint8_t>[zSize];

                for (size_t kk = 0; kk < zSize; ++kk)
                {
                    pppDependencies[ii][jj][kk].store((ii > 0) + (jj > 0) + (kk > 0), memory_order::relaxed);
                }
            }
        }

        dep_arrary_type depArray(pppDependencies, xSize, ySize, zSize);

        //
        // Launch root task and wait.

        uint32_t workerCount = microScheduler.workerCount();
        partitioner.template initialize<TRange<TArgs...>>((uint16_t)workerCount);

        Task* pTask = microScheduler.allocateTask<TWavefrontTask>(
            wavefrontFunc, pUserData, range, partitioner, priority);

        microScheduler.spawnTaskAndWait(pTask, priority);

        //
        // Delete the array of dependency counters.

        for (size_t ii = 0; ii < xSize; ++ii)
        {
            for (size_t jj = 0; jj < ySize; ++jj)
            {
                delete[] pppDependencies[ii][jj];
            }
            delete[] pppDependencies[ii];
        }
        delete[] pppDependencies;
    }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A construct that maps a parallel-wavefront behavior to a MicroScheduler.
 *
 * @todo Implement 3D version.
 * @todo Cleanup and Optimize
 */
class ParallelWavefront
{
public:

    /**
     * @brief
     *  Creates a ParallelWavefront object bound to the specified 'scheduler'. All
     *  parallel-wavefront operations will be scheduled with the specified 'priority'.
     */
    GTS_INLINE ParallelWavefront(MicroScheduler& scheduler, uint32_t priority = 0)
        : m_microScheduler(scheduler)
        , m_priority(priority)
    {}

    /**
     * @brief
     *  Applies the given function 'wavefrontFunc' to the specified iteration 'range'.
     * @param range
     *  An iteration range.
     * @param wavefrontFunc
     *  The wavefront function to apply to each range block. Signature:
     * @code
     *  void(*)(TRange& range, void* pUserData, TaskContext const&);
     * @endcode
     * @param partitioner
     *  The partitioning algorithmic that will subdivided range.
     * @param pUserData
     *  Optional user data that will be passed into func.
     */
    template<
        typename TFunc,
        typename TPartitioner,
        template<typename...> class TRange,
        class... TArgs
    >
    GTS_INLINE void operator()(
        TRange<TArgs...> const& range,
        TFunc wavefrontFunc,
        TPartitioner partitioner,
        void* pUserData)
    {
        GTS_ASSERT(m_microScheduler.isRunning());

        using dep_arrary_type = DependencyArray<TRange<TArgs...>, TRange<size_t>, TRange<TArgs...>::DIMENSIONALITY>;

        LaunchTask<TRange<TArgs...>::DIMENSIONALITY>::template launch<ParallelWavefrontTask<TFunc, TRange<TArgs...>, TPartitioner, dep_arrary_type>>(
            m_microScheduler, m_priority, range, wavefrontFunc, partitioner, pUserData);

        #pragma message ("ParallelWavefront is a WIP. It may break or be slow.")
    }

private:

    MicroScheduler& m_microScheduler;
    uint32_t m_priority;

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    class TheftObserverTask : public Task
    {
    public:

        //----------------------------------------------------------------------
        GTS_INLINE TheftObserverTask()
            : m_childTaskStolen(false)
        {}

        //----------------------------------------------------------------------
        GTS_INLINE virtual Task* execute(TaskContext const&) final
        {
            return nullptr;
        }

        gts::Atomic<bool> m_childTaskStolen;
    };

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    template<typename TFunc, typename TRange, typename TPartitioner, typename TDependencyArray>
    class ParallelWavefrontTask : public Task
    {
    public:

        //----------------------------------------------------------------------
        ParallelWavefrontTask(
            TFunc& func,
            void* userData,
            TRange const& range,
            TPartitioner partitioner,
            uint32_t priority,
            TDependencyArray& depArray)

            : m_range(range)
            , m_partitioner(partitioner)
            , m_func(func)
            , m_pUserData(userData)
            , m_depArray(depArray)
            , m_priority(priority)
        {}

        //----------------------------------------------------------------------
        ParallelWavefrontTask(ParallelWavefrontTask& parent, TRange const& range)
            : m_range(range)
            , m_partitioner(parent.m_partitioner, 0, TRange())
            , m_func(parent.m_func)
            , m_pUserData(parent.m_pUserData)
            , m_depArray(parent.m_depArray)
            , m_priority(parent.m_priority)
        {}

        //----------------------------------------------------------------------
        ParallelWavefrontTask(ParallelWavefrontTask& parent, TRange const& range, uint16_t depth)
            : m_range(range)
            , m_partitioner(parent.m_partitioner, depth, TRange())
            , m_func(parent.m_func)
            , m_pUserData(parent.m_pUserData)
            , m_depArray(parent.m_depArray)
            , m_priority(parent.m_priority)
        {}

        //----------------------------------------------------------------------
        ParallelWavefrontTask(ParallelWavefrontTask const&) = default;

        //----------------------------------------------------------------------
        ParallelWavefrontTask& operator=(ParallelWavefrontTask const& other) = default;

        //----------------------------------------------------------------------
        void offerRange(TaskContext const& ctx, TRange const* pReadySplits, uint8_t numReadySplits)
        {
            // Allocate the continuation.
            Task* pContinuation = ctx.pMicroScheduler->allocateTask<TheftObserverTask>();
            pContinuation->addRef(numReadySplits, gts::memory_order::relaxed);
            setContinuationTask(pContinuation);

            // Add the splits as siblings.
            for (int iChild = 0; iChild < numReadySplits; ++iChild)
            {
                Task* pChild = ctx.pMicroScheduler->allocateTask<ParallelWavefrontTask>(
                    *this, pReadySplits[iChild]);

                pContinuation->addChildTaskWithoutRef(pChild);
                ctx.pMicroScheduler->spawnTask(pChild);
            }

            m_partitioner.template split<TRange>();
        }

        //----------------------------------------------------------------------
        void offerRange(TaskContext const& ctx, TRange const& range, uint16_t depth = 0)
        {
            GTS_ASSERT(!range.empty() && "Bug in partitioner!");

            // Allocate the continuation.
            Task* pContinuation = ctx.pMicroScheduler->allocateTask<TheftObserverTask>();
            pContinuation->addRef(2, gts::memory_order::relaxed);
            setContinuationTask(pContinuation);

            // The split is the right sibling.
            Task* pSibling = ctx.pMicroScheduler->allocateTask<ParallelWavefrontTask>(*this, range, depth);
            pContinuation->addChildTaskWithoutRef(pSibling);
            ctx.pMicroScheduler->spawnTask(pSibling);

            // This task becomes a sibling.
            pContinuation->addChildTaskWithoutRef(this);

            m_partitioner.template split<TRange>();
        }

        //----------------------------------------------------------------------
        template<typename TSplitter>
        void run(TaskContext const& ctx, TRange& range, TSplitter const& splitter)
        {
            // Run this range.
            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WAVEFRONT, analysis::Color::AntiqueWhite, "Wavefront::run", range.xRange().size(), range.yRange().size());
            m_func(range, m_pUserData, ctx);

            // Collect ready neighbors.
            TRange readySplits[TRange::MAX_SPLITS * 2];
            uint8_t numReadySplits = 0;
            m_depArray.produceSplits(splitter, range, readySplits, numReadySplits);

            // Offer up the ready neighbors.
            if(numReadySplits > 0)
            {
                offerRange(ctx, readySplits, numReadySplits);
            }
        }

        //----------------------------------------------------------------------
        Task* execute(TaskContext const& ctx)
        {
            GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::WAVEFRONT, analysis::Color::RoyalBlue, isStolen() ? "S" : "P");

            return m_partitioner.execute(ctx, this, m_range);
        }

        //----------------------------------------------------------------------
        void initialOffer(TaskContext const&, TRange& range, typename TPartitioner::splitter_type const& splitter)
        {
            typename TRange::split_result splits;
            range.split(splits, splitter);

            m_partitioner.template split<TRange>();
        }

        //----------------------------------------------------------------------
        void balanceAndExecute(TaskContext const& ctx, TPartitioner& partitioner, TRange& range, typename TPartitioner::splitter_type const& splitter)
        {
            uint16_t splitDepth = 0;

            // Split to depth to balance the Task tree.
            while (splitDepth < partitioner.maxBalancedSplitDepth() && range.isDivisible())
            {
                GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::PaleVioletRed4, "ADAPTIVE SPLIT");

                typename TRange::split_result splits;
                range.split(splits, splitter);
                ++splitDepth;
            }

            // Run this range.
            TRange readySplits[TRange::MAX_SPLITS * 2];
            uint8_t numReadySplits = 0;
            m_func(range, m_pUserData, ctx);

            // Collect ready neighbors.
            m_depArray.produceSplits(splitter, range, readySplits, numReadySplits);

            if (numReadySplits > 0)
            {
                // Spawn ready neighbors.
                for (size_t ii = 0; ii < numReadySplits; ++ii)
                {
                    offerRange(ctx, readySplits[ii], partitioner.maxBalancedSplitDepth() - 1);
                }
            }
        }

    private:

        using split_result = typename TRange::split_result;

        TRange            m_range;
        TPartitioner      m_partitioner;
        TFunc&            m_func;
        void*             m_pUserData;
        TDependencyArray  m_depArray;
        uint32_t          m_priority;
    };
};

/** @} */ // end of ParallelPatterns
/** @} */ // end of MicroScheduler

} // namespace gts
