#pragma once

#include <library/cpp/containers/stack_vector/stack_vec.h>

#include <util/generic/bitops.h>

#include <cmath>

#include "percentile_base.h"

namespace NMonitoring {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Percentile tracker for monitoring
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <size_t BASE_BITS, size_t EXP_BITS, size_t FRAME_COUNT>
struct TPercentileTrackerLg : public TPercentileBase {
    static constexpr size_t BUCKET_COUNT = size_t(1) << EXP_BITS;
    static constexpr size_t BUCKET_SIZE = size_t(1) << BASE_BITS;
    static constexpr size_t ITEMS_COUNT = BUCKET_COUNT * BUCKET_SIZE;
    static constexpr size_t TRACKER_LIMIT = BUCKET_SIZE * ((size_t(1) << BUCKET_COUNT) - 1)
                                            - (size_t(1) << (BUCKET_COUNT - 1));
    static constexpr size_t MAX_GRANULARITY = size_t(1) << (BUCKET_COUNT - 1);

    size_t Borders[BUCKET_COUNT];
    TAtomic Items[ITEMS_COUNT];
    TAtomicBase Frame[FRAME_COUNT][ITEMS_COUNT];
    size_t CurrentFrame;

    TPercentileTrackerLg()
        : CurrentFrame(0)
    {
        Borders[0] = 0;
        for (size_t i = 1; i < BUCKET_COUNT; ++i) {
            Borders[i] = Borders[i-1] + (BUCKET_SIZE << (i - 1));
        }
        for (size_t i = 0; i < ITEMS_COUNT; ++i) {
            AtomicSet(Items[i], 0);
        }
        for (size_t frame = 0; frame < FRAME_COUNT; ++frame) {
            for (size_t bucket = 0; bucket < ITEMS_COUNT; ++bucket) {
                Frame[frame][bucket] = 0;
            }
        }
    }

    size_t inline BucketIdxIf(size_t value) {
        static_assert(BASE_BITS == 5, "if-based bucket calculation cannot be used if BASE_BITS != 5");
        size_t bucket_idx;
        if (value < 8160) {
            if (value < 480) {
                if (value < 96) {
                    if (value < 32) {
                        bucket_idx = 0;
                    } else {
                        bucket_idx = 1;
                    }
                } else {
                    if (value < 224) {
                        bucket_idx = 2;
                    } else {
                        bucket_idx = 3;
                    }
                }
            } else {
                if (value < 2016) {
                    if (value < 992) {
                        bucket_idx = 4;
                    } else {
                        bucket_idx = 5;
                    }
                } else {
                    if (value < 4064) {
                        bucket_idx = 6;
                    } else {
                        bucket_idx = 7;
                    }
                }
            }
        } else {
            if (value < 131040) {
                if (value < 32736) {
                    if (value < 16352) {
                        bucket_idx = 8;
                    } else {
                        bucket_idx = 9;
                    }
                } else {
                    if (value < 65504) {
                        bucket_idx = 10;
                    } else {
                        bucket_idx = 11;
                    }
                }
            } else {
                if (value < 524256) {
                    if (value < 262112) {
                        bucket_idx = 12;
                    } else {
                        bucket_idx = 13;
                    }
                } else {
                    if (value < 1048544) {
                        bucket_idx = 14;
                    } else {
                        bucket_idx = 15;
                    }
                }
            }
        }
        return Min(bucket_idx, BUCKET_COUNT - 1);
    }

    size_t inline BucketIdxBinarySearch(size_t value) {
        size_t l = 0;
        size_t r = BUCKET_COUNT;
        while (l < r - 1) {
            size_t mid = (l + r) / 2;
            if (value < Borders[mid]) {
                r = mid;
            } else {
                l = mid;
            }
        }
        return l;
    }

    size_t inline BucketIdxMostSignificantBit(size_t value) {
        size_t bucket_idx = MostSignificantBit(value + BUCKET_SIZE) - BASE_BITS;
        return Min(bucket_idx, BUCKET_COUNT - 1);
    }

    void Increment(size_t value) {
        size_t bucket_idx = BucketIdxMostSignificantBit(value);
        size_t inside_bucket_idx = (value - Borders[bucket_idx] + (1 << bucket_idx) - 1) >> bucket_idx;
        size_t idx = bucket_idx * BUCKET_SIZE + inside_bucket_idx;
        AtomicIncrement(Items[Min(idx, ITEMS_COUNT - 1)]);
    }

    // Needed only for tests
    size_t GetPercentile(float threshold) {
        TStackVec<TAtomicBase, ITEMS_COUNT> totals(ITEMS_COUNT);
        TAtomicBase total = 0;
        for (size_t i = 0; i < ITEMS_COUNT; ++i) {
            total += AtomicGet(Items[i]);
            totals[i] = total;
        }
        TAtomicBase item_threshold = std::llround(threshold * (float)total);
        item_threshold = Min(item_threshold, total);
        auto it = LowerBound(totals.begin(), totals.end(), item_threshold);
        size_t index = it - totals.begin();
        size_t bucket_idx = index / BUCKET_SIZE;
        return Borders[bucket_idx] + ((index % BUCKET_SIZE) << bucket_idx);
    }

    // shift frame (call periodically)
    void Update() {
        TStackVec<TAtomicBase, ITEMS_COUNT> totals(ITEMS_COUNT);
        TAtomicBase total = 0;
        for (size_t i = 0; i < ITEMS_COUNT; ++i) {
            TAtomicBase item = AtomicGet(Items[i]);
            TAtomicBase prevItem = Frame[CurrentFrame][i];
            Frame[CurrentFrame][i] = item;
            total += item - prevItem;
            totals[i] = total;
        }

        for (size_t i = 0; i < Percentiles.size(); ++i) {
            TPercentile &percentile = Percentiles[i];
            TAtomicBase threshold = std::llround(percentile.first * (float)total);
            threshold = Min(threshold, total);
            auto it = LowerBound(totals.begin(), totals.end(), threshold);
            size_t index = it - totals.begin();
            size_t bucket_idx = index / BUCKET_SIZE;
            (*percentile.second) = Borders[bucket_idx] + ((index % BUCKET_SIZE) << bucket_idx);
        }
        CurrentFrame = (CurrentFrame + 1) % FRAME_COUNT;
    }
};

} // NMonitoring
