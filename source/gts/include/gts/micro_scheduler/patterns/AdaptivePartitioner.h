/*******************************************************************************
 * Copyright (c) 2005-2021 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/
#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

// Derivative of TBB auto_partitioner. https://github.com/intel/tbb

///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  Adaptively subdivides a TRange based on demand from the scheduler.
 */
class AdaptivePartitioner
{
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // A deque of retained subdivided ranges that are ready to be offered to
    // the scheduler when there is demand from other Workers. Ranges are executed
    // off the back and shared off the front.
    template<typename TRange, typename TSplitter>
    class RetentionDeque
    {
    public:

        using split_result = typename TRange::split_result;

        //----------------------------------------------------------------------
        GTS_INLINE RetentionDeque()
        {}

        //----------------------------------------------------------------------
        GTS_INLINE RetentionDeque(TRange const& range)
        {
            m_rangeRingBuf[m_back & MASK] = range;
            m_depthRingBuf[m_back & MASK] = 0;
            ++m_back;
        }

        //----------------------------------------------------------------------
        GTS_INLINE void popFront()
        {
            m_rangeRingBuf[m_front & MASK].~TRange();
            ++m_front;
        }

        //----------------------------------------------------------------------
        GTS_INLINE void popBack()
        {
            m_rangeRingBuf[(m_back - 1) & MASK].~TRange();
            --m_back;
        }

        //----------------------------------------------------------------------
        GTS_INLINE void pushBack(TRange& range, uint16_t depth)
        {
            m_rangeRingBuf[m_back & MASK] = range;
            m_depthRingBuf[m_back & MASK] = depth;
            ++m_back;
        }

        //----------------------------------------------------------------------
        GTS_INLINE TRange& front()
        {
            return m_rangeRingBuf[m_front & MASK];
        }

        //----------------------------------------------------------------------
        GTS_INLINE TRange& back()
        {
            return m_rangeRingBuf[(m_back - 1) & MASK];
        }

        //----------------------------------------------------------------------
        GTS_INLINE uint16_t frontDepth()
        {
            return m_depthRingBuf[m_front & MASK];
        }

        //----------------------------------------------------------------------
        GTS_INLINE uint16_t backDepth()
        {
            return m_depthRingBuf[(m_back - 1) & MASK];
        }

        //----------------------------------------------------------------------
        GTS_INLINE int16_t size() const
        {
            return m_back - m_front;
        }

        //----------------------------------------------------------------------
        GTS_INLINE bool empty() const
        {
            return size() == 0;
        }

        //----------------------------------------------------------------------
        GTS_INLINE bool full() const
        {
            return size() == MASK + 1;
        }

        //----------------------------------------------------------------------
        GTS_INLINE bool isDivisible(int16_t maxDepth)
        {
            return size() < MASK && backDepth() < maxDepth && back().isDivisible();
        }

        //----------------------------------------------------------------------
        GTS_NO_INLINE void splitToDepth(int16_t maxDepth)
        {
            TRange range = back();
            uint16_t depth = backDepth();
            popBack();

            while (size() - 1 + TRange::MAX_SPLITS < MASK
                && depth < maxDepth
                && range.isDivisible())
            {
                split_result splits;
                range.split(splits, TSplitter());
                ++depth;

                for (int ii = 0; ii < splits.size; ++ii)
                {
                    m_rangeRingBuf[m_back & MASK] = splits.ranges[ii];
                    m_depthRingBuf[m_back & MASK] = depth;
                    ++m_back;
                }
            }
            m_rangeRingBuf[m_back & MASK] = range;
            m_depthRingBuf[m_back & MASK] = depth;
            ++m_back;
        }

    private:

        enum
        {
            MASK = gtsMax(TRange::SPLIT_FACTOR * 2, (size_t)8) - 1
        };

        TRange m_rangeRingBuf[MASK + 1];
        uint16_t m_depthRingBuf[MASK + 1];
        int16_t m_front = 0;
        int16_t m_back = 0;
    };

public:

    using splitter_type = EvenSplitter;

    //--------------------------------------------------------------------------
    /**
     * @brief
     *  Constructs an AdaptivePartitioner object and defines how deep is can
     *  split a Range object.
     * @param initialMaxSplitDepth
     *  Defines how deep the initial binary tree generated by subdividing the
     *  a Range can get. The default value is good for most situations. The caller
     *  can increase the value for more irregular workloads, or decrease the value
     *  for more regular workloads.
     */
    explicit GTS_INLINE AdaptivePartitioner(uint16_t initialMaxSplitDepth = 4)
        : m_initialSplitDepth(0)
        , m_maxBalancedSplitDepth(initialMaxSplitDepth)
        , m_numSiblingThefts(0)
    {}

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE AdaptivePartitioner(AdaptivePartitioner& other, uint16_t depth, TRange const&)
        : m_initialSplitDepth(getSplit<TRange>(other))
        , m_maxBalancedSplitDepth(other.m_maxBalancedSplitDepth)
        , m_numSiblingThefts(0)
    {
        GTS_ASSERT(depth <= m_maxBalancedSplitDepth);

        // Don't split past the depth of our sibling.
        m_maxBalancedSplitDepth -= depth;
    }

    //--------------------------------------------------------------------------
    template<typename TPattern, typename TRange>
    GTS_INLINE Task* execute(TaskContext const& ctx, TPattern* pPattern, TRange& range)
    {
        // Divide up the range while its minimum size has not been reached. Spawn
        // each division as new continuation and sibling tasks.
        while (range.isDivisible() && isDivisible())
        {
            pPattern->initialOffer(ctx, range, splitter_type());
        }

        return doExecute(ctx, pPattern, range, splitter_type());
    }

    //--------------------------------------------------------------------------
    template<typename TPattern, typename TRange>
    GTS_INLINE void initialOffer(TaskContext const& ctx, TPattern* pPattern, TRange& range, splitter_type const& splitter)
    {
        pPattern->offerRange(ctx, range, splitter);
    }

    //--------------------------------------------------------------------------
    template<typename TPattern, typename TRange>
    GTS_INLINE Task* doExecute(TaskContext const& ctx, TPattern* pPattern, TRange& range, splitter_type const& splitter)
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::RoyalBlue1, "ADAPTIVE doExecute", m_maxBalancedSplitDepth);

        if (!range.isDivisible() || m_maxBalancedSplitDepth == 0)
        {
            GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::RoyalBlue1, "ADAPTIVE RUN", pPattern);

            // Execute any remaining work.
            pPattern->run(ctx, range, splitter);
        }
        else
        {
            GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::RoyalBlue2, "ADAPTIVE DEMAND", pPattern);
            pPattern->balanceAndExecute(ctx, *this, range, splitter);
        }

        GTS_SIM_TRACE_MARKER(sim_trace::MARKER_PARTITIONER_END);
        return nullptr;
    }

    //--------------------------------------------------------------------------
    template<typename TPattern, typename TRange>
    GTS_INLINE void balanceAndExecute(
        TaskContext const& ctx,
        TPattern* pPattern,
        TRange& range,
        splitter_type const& splitter)
    {
        RetentionDeque<TRange, splitter_type> retentionDeque(range);

        do
        {
            retentionDeque.splitToDepth(m_maxBalancedSplitDepth);

            // If the MicroScheduler has demand for work, try to offer work.
            if (hasDemand<TRange>(ctx))
            {
                // Only offer a new task if there is more than one range in
                // the deque.
                if (retentionDeque.size() > 1)
                {
                    pPattern->offerRange(ctx, retentionDeque.front(), retentionDeque.frontDepth());
                    retentionDeque.popFront();
                    continue;
                }

                if (retentionDeque.isDivisible(m_maxBalancedSplitDepth))
                {
                    continue;
                }
            }

            pPattern->run(ctx, retentionDeque.back(), splitter);
            retentionDeque.popBack();

        } while (!retentionDeque.empty());
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE bool hasDemand(TaskContext const& ctx)
    {
        GTS_TRACE_ZONE_MARKER_P0(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::PaleVioletRed3, "DOES THE SYSTEM HAVE DEMAND?");

        // Check for demand on the scheduler.
        if (ctx.pMicroScheduler->hasDemand(true))
        {
            GTS_TRACE_ZONE_MARKER_P0(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Green, "YES");
            ++m_maxBalancedSplitDepth;
            return true;
        }

        return false;
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE void adjustIfStolen(Task* pThisTask)
    {
        // Only consider Tasks that are not part of the initial split phase.
        if (m_initialSplitDepth == 0)
        {
            // Allow the Task to split to m_maxBalancedSplitDepth.
            m_initialSplitDepth = 1;

            if (pThisTask->isStolen() &&
                // Communicate the theft only if there is another child.
                pThisTask->parent()->refCount() > 2)
            {
                GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::PaleVioletRed2, "NOFIFY STOLEN");

                if (m_maxBalancedSplitDepth == 0)
                {
                    increaseDepth<TRange>();
                };

                increaseDepth<TRange>();
            }
        }
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE void initialize(uint16_t workerCount)
    {
        GTS_ASSERT(workerCount > 0);
        m_initialSplitDepth = TRange::adjustDivisor(nextPow2(workerCount), false);
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE void split()
    {
        m_initialSplitDepth = TRange::splitInitialDepth(m_initialSplitDepth, false);
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    static GTS_INLINE uint16_t getSplit(AdaptivePartitioner& partitioner)
    {
        return TRange::splitInitialDepth(partitioner.m_initialSplitDepth, false);
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE void increaseDepth()
    {
        // Increment the depth if the number of sibling thefts is beyond the
        // maximum split threshold. We do this so that a depth increment represents
        // all children for the next depth.
        if (++m_numSiblingThefts >= TRange::MAX_SPLITS)
        {
            ++m_maxBalancedSplitDepth;
            m_numSiblingThefts = 0;
        }
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool isDivisible()
    {
        return m_initialSplitDepth > 1;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE uint16_t maxBalancedSplitDepth() const
    {
        return m_maxBalancedSplitDepth;
    }

private:

    //! The number of initial splits encoded as the number of leaves at the
    //! maximum depth of a (TRange::MAX_SPLITS)-tree. That is, splits equals
    //! log_TRange::MAX_SPLITS(m_initialSplitDepth).
    uint16_t m_initialSplitDepth;

    //! The current maximum depth needed to produce a balanced task tree.
    uint16_t m_maxBalancedSplitDepth;

    //! The number of sibling Task thefts between depth increments.
    uint16_t m_numSiblingThefts;
};


#ifdef GTS_MSVC
#pragma warning( pop )
#endif