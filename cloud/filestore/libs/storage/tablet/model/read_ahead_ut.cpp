#include "read_ahead.h"

#include <library/cpp/testing/unittest/registar.h>

#include <util/generic/vector.h>
#include <util/string/printf.h>

namespace NCloud::NFileStore::NStorage {

namespace {

////////////////////////////////////////////////////////////////////////////////

struct TDefaultCache: TReadAheadCache
{
    static constexpr ui32 MaxNodes = 1024;
    static constexpr ui32 MaxResultsPerNode = 32;
    static constexpr ui32 RangeSize = 1_MB;
    static constexpr ui32 MaxGap = 20;

    TDefaultCache()
        : TReadAheadCache(TDefaultAllocator::Instance())
    {
        Reset(MaxNodes, MaxResultsPerNode, RangeSize, MaxGap);
    }
};

////////////////////////////////////////////////////////////////////////////////

TByteRange MakeRange(ui64 offset, ui32 len)
{
    return TByteRange(offset, len, 4_KB);
}

void RegisterResult(TDefaultCache& cache, ui64 nodeId, ui64 offset, ui32 len)
{
    NProtoPrivate::TDescribeDataResponse result;
    auto* p = result.AddBlobPieces();
    p->SetBSGroupId(1);
    p->MutableBlobId()->SetRawX1(1);
    p->MutableBlobId()->SetRawX2(2);
    p->MutableBlobId()->SetRawX3(3);
    auto* range = p->AddRanges();
    range->SetOffset(offset);
    range->SetLength(len);
    range->SetBlobOffset(0);

    cache.RegisterResult(nodeId, MakeRange(offset, len), result);
};

TString Expected(
    ui64 nodeId,
    ui64 offset,
    ui32 len,
    ui64 x1,
    ui64 x2,
    ui64 x3,
    ui32 groupId,
    ui32 blobOffset)
{
    return Sprintf(
        "n=%lu,o=%lu,l=%u,blob=%lu/%lu/%lu,g=%u,bo=%u",
        nodeId,
        offset,
        len,
        x1,
        x2,
        x3,
        groupId,
        blobOffset);
}

TString Expected(ui64 nodeId, ui64 offset, ui32 len, ui32 blobOffset)
{
    return Expected(nodeId, offset, len, 1, 2, 3, 1, blobOffset);
}

TString FillResult(TDefaultCache& cache, ui64 nodeId, ui64 offset, ui32 len)
{
    NProtoPrivate::TDescribeDataResponse result;
    if (cache.TryFillResult(nodeId, MakeRange(offset, len), &result)) {
        const auto& bps = result.GetBlobPieces();
        UNIT_ASSERT_VALUES_EQUAL(1, bps.size());
        const auto& branges = bps[0].GetRanges();
        UNIT_ASSERT_VALUES_EQUAL(1, branges.size());
        return Expected(
            nodeId,
            branges[0].GetOffset(),
            branges[0].GetLength(),
            bps[0].GetBlobId().GetRawX1(),
            bps[0].GetBlobId().GetRawX2(),
            bps[0].GetBlobId().GetRawX3(),
            bps[0].GetBSGroupId(),
            branges[0].GetBlobOffset());
    }
    return {};
};

}   // namespace

////////////////////////////////////////////////////////////////////////////////

Y_UNIT_TEST_SUITE(TReadAheadTest)
{
    Y_UNIT_TEST(ShouldDetectPureSequentialRead)
    {
        const ui64 nodeId = 111;
        const ui32 blockSize = 4_KB;
        const ui32 requestSize = 32 * blockSize;

        TDefaultCache cache;

        TMaybe<TByteRange> r;
        ui64 offset = 0;
        while (offset < 1_MB - requestSize) {
            r = cache.RegisterDescribe(
                nodeId,
                TByteRange(offset, requestSize, blockSize));
            UNIT_ASSERT_C(!r, r.GetRef().Describe());
            offset += requestSize;
        }

        while (offset < 10_MB) {
            r = cache.RegisterDescribe(
                nodeId,
                TByteRange(offset, requestSize, blockSize));
            UNIT_ASSERT(r);
            UNIT_ASSERT_VALUES_EQUAL(
                TByteRange(offset, 1_MB, blockSize).Describe(),
                r->Describe());
            offset += requestSize;
        }

        r = cache.RegisterDescribe(
            nodeId,
            TByteRange(100_MB, requestSize, blockSize));
        UNIT_ASSERT_C(!r, r.GetRef().Describe());
    }

    Y_UNIT_TEST(ShouldDetectAlmostSequentialRead)
    {
        const ui64 nodeId = 111;
        const ui32 blockSize = 4_KB;

        TDefaultCache cache;
        TMaybe<TByteRange> r;
        r = cache.RegisterDescribe(nodeId, TByteRange(0, 128_KB, blockSize));
        UNIT_ASSERT_C(!r, r.GetRef().Describe());
        r = cache.RegisterDescribe(nodeId, TByteRange(128_KB, 128_KB, blockSize));
        UNIT_ASSERT_C(!r, r.GetRef().Describe());
        r = cache.RegisterDescribe(nodeId, TByteRange(512_KB, 256_KB, blockSize));
        UNIT_ASSERT_C(!r, r.GetRef().Describe());
        r = cache.RegisterDescribe(nodeId, TByteRange(384_KB, 128_KB, blockSize));
        UNIT_ASSERT_C(!r, r.GetRef().Describe());
        r = cache.RegisterDescribe(nodeId, TByteRange(768_KB, 256_KB, blockSize));
        UNIT_ASSERT(r);
        UNIT_ASSERT_VALUES_EQUAL(
            TByteRange(768_KB, 1_MB, blockSize).Describe(),
            r->Describe());
        r = cache.RegisterDescribe(
            nodeId,
            TByteRange(1_MB + 256_KB, 256_KB, blockSize));
        UNIT_ASSERT_C(!r, r.GetRef().Describe());
        r = cache.RegisterDescribe(
            nodeId,
            TByteRange(1_MB + 512_KB, 384_KB, blockSize));
        UNIT_ASSERT(r);
        UNIT_ASSERT_VALUES_EQUAL(
            TByteRange(1_MB + 512_KB, 1_MB, blockSize).Describe(),
            r->Describe());
    }

    Y_UNIT_TEST(ShouldCacheResults)
    {
        TDefaultCache cache;

        RegisterResult(cache, 111, 0, 1_MB);
        RegisterResult(cache, 111, 1_MB, 1_MB);
        RegisterResult(cache, 111, 2_MB, 1_MB);
        RegisterResult(cache, 222, 100_MB, 1_MB);
        RegisterResult(cache, 222, 105_MB, 1_MB);

        UNIT_ASSERT_VALUES_EQUAL("", FillResult(cache, 333, 0, 1_MB));

        UNIT_ASSERT_VALUES_EQUAL(
            Expected(111, 0, 128_KB, 0),
            FillResult(cache, 111, 0, 128_KB));
        UNIT_ASSERT_VALUES_EQUAL(
            Expected(111, 1_MB - 128_KB, 128_KB, 1_MB - 128_KB),
            FillResult(cache, 111, 1_MB - 128_KB, 128_KB));
        UNIT_ASSERT_VALUES_EQUAL(
            Expected(111, 1_MB, 128_KB, 0),
            FillResult(cache, 111, 1_MB, 128_KB));
        UNIT_ASSERT_VALUES_EQUAL(
            Expected(111, 2_MB - 128_KB, 128_KB, 1_MB - 128_KB),
            FillResult(cache, 111, 2_MB - 128_KB, 128_KB));
        UNIT_ASSERT_VALUES_EQUAL(
            Expected(111, 2_MB, 128_KB, 0),
            FillResult(cache, 111, 2_MB, 128_KB));
        UNIT_ASSERT_VALUES_EQUAL(
            Expected(111, 3_MB - 128_KB, 128_KB, 1_MB - 128_KB),
            FillResult(cache, 111, 3_MB - 128_KB, 128_KB));
        UNIT_ASSERT_VALUES_EQUAL("", FillResult(cache, 111, 3_MB, 128_KB));

        UNIT_ASSERT_VALUES_EQUAL(
            Expected(222, 100_MB, 128_KB, 0),
            FillResult(cache, 222, 100_MB, 128_KB));
        UNIT_ASSERT_VALUES_EQUAL(
            Expected(222, 101_MB - 128_KB, 128_KB, 1_MB - 128_KB),
            FillResult(cache, 222, 101_MB - 128_KB, 128_KB));
        UNIT_ASSERT_VALUES_EQUAL("", FillResult(cache, 222, 101_MB, 128_KB));
        UNIT_ASSERT_VALUES_EQUAL(
            "",
            FillResult(cache, 222, 105_MB - 128_KB, 128_KB));
        UNIT_ASSERT_VALUES_EQUAL(
            Expected(222, 105_MB, 128_KB, 0),
            FillResult(cache, 222, 105_MB, 128_KB));
        UNIT_ASSERT_VALUES_EQUAL(
            Expected(222, 106_MB - 128_KB, 128_KB, 1_MB - 128_KB),
            FillResult(cache, 222, 106_MB - 128_KB, 128_KB));
    }

    Y_UNIT_TEST(ShouldEvictNodesAndResults)
    {
        TDefaultCache cache;

        ui64 nodeId = 1;
        while (nodeId < TDefaultCache::MaxNodes + 1) {
            for (ui32 rangeId = 0;
                    rangeId < 2 * TDefaultCache::MaxResultsPerNode; ++rangeId) {
                RegisterResult(cache, nodeId, rangeId * 1_MB, 1_MB);
            }

            ++nodeId;
        }

        UNIT_ASSERT_VALUES_EQUAL(
            TDefaultCache::MaxNodes,
            cache.GetStats().NodeCount);

        while (nodeId < 2 * TDefaultCache::MaxNodes + 1) {
            for (ui32 rangeId = 0;
                    rangeId < 2 * TDefaultCache::MaxResultsPerNode; ++rangeId) {
                RegisterResult(cache, nodeId, rangeId * 1_MB, 1_MB);
            }

            ++nodeId;
        }

        const ui64 firstNodeId = nodeId - TDefaultCache::MaxNodes;
        const ui64 lastNodeId = nodeId - 1;
        const ui64 firstOffset = TDefaultCache::MaxResultsPerNode * 1_MB;
        const ui64 lastOffset =
            (2 * TDefaultCache::MaxResultsPerNode - 1) * 1_MB;

        // nothing should be cached for the nodes with id < firstNodeId
        UNIT_ASSERT_VALUES_EQUAL(
            "",
            FillResult(cache, 1, lastOffset, 1_MB));
        UNIT_ASSERT_VALUES_EQUAL(
            "",
            FillResult(cache, firstNodeId - 1, lastOffset, 1_MB));

        // nothing should be cached for the ranges with offsets < firstOffset
        UNIT_ASSERT_VALUES_EQUAL("", FillResult(cache, firstNodeId, 0, 1_MB));
        UNIT_ASSERT_VALUES_EQUAL("", FillResult(
            cache,
            firstNodeId,
            (firstOffset - 1_MB),
            1_MB));
        UNIT_ASSERT_VALUES_EQUAL("", FillResult(cache, lastNodeId, 0, 1_MB));
        UNIT_ASSERT_VALUES_EQUAL("", FillResult(
            cache,
            lastNodeId,
            (firstOffset - 1_MB),
            1_MB));

        // ranges with offsets >= firstOffsets for the nodes with
        // id >= firstNodeId should be cached

        UNIT_ASSERT_VALUES_EQUAL(
            Expected(firstNodeId, firstOffset, 1_MB, 0),
            FillResult(cache, firstNodeId, firstOffset, 1_MB));
        UNIT_ASSERT_VALUES_EQUAL(
            Expected(firstNodeId, lastOffset, 1_MB, 0),
            FillResult(cache, firstNodeId, lastOffset, 1_MB));
        UNIT_ASSERT_VALUES_EQUAL(
            Expected(lastNodeId, firstOffset, 1_MB, 0),
            FillResult(cache, lastNodeId, firstOffset, 1_MB));
        UNIT_ASSERT_VALUES_EQUAL(
            Expected(lastNodeId, lastOffset, 1_MB, 0),
            FillResult(cache, lastNodeId, lastOffset, 1_MB));
    }

    Y_UNIT_TEST(ShouldFilterResult)
    {
        auto makeContent = [] (ui64 offset, ui32 len) {
            TString content(len, 0);
            for (ui32 i = 0; i < len; ++i) {
                content[i] = 'a' + (offset + i) % ('z' - 'a' + 1);
            }
            return content;
        };

        auto makeFresh = [=] (ui64 offset, ui32 len) {
            NProtoPrivate::TFreshDataRange fresh;
            fresh.SetOffset(offset);
            *fresh.MutableContent() = makeContent(offset, len);
            return fresh;
        };

        NProtoPrivate::TDescribeDataResponse src;
        src.SetFileSize(100_MB);

        *src.AddFreshDataRanges() = makeFresh(10_MB + 10_KB, 1_KB);
        *src.AddFreshDataRanges() = makeFresh(10_MB + 100_KB, 7_KB);
        *src.AddFreshDataRanges() = makeFresh(10_MB + 127_KB, 3_KB);
        *src.AddFreshDataRanges() = makeFresh(10_MB + 512_KB, 64_KB);

        auto makeBlobPiece = [] (ui32 x1, ui32 x2, ui32 x3, ui32 groupId) {
            NProtoPrivate::TBlobPiece piece;
            piece.SetBSGroupId(groupId);
            auto* blobId = piece.MutableBlobId();
            blobId->SetRawX1(x1);
            blobId->SetRawX2(x2);
            blobId->SetRawX3(x3);
            return piece;
        };

        auto makeBlobRange = [] (ui64 offset, ui32 len, ui32 blobOffset)
        {
            NProtoPrivate::TRangeInBlob blobRange;
            blobRange.SetOffset(offset);
            blobRange.SetLength(len);
            blobRange.SetBlobOffset(blobOffset);
            return blobRange;
        };

        *src.AddBlobPieces() = makeBlobPiece(1, 2, 3, 10);
        *src.MutableBlobPieces(0)->AddRanges() =
            makeBlobRange(10_MB, 256_KB, 512_KB);
        *src.MutableBlobPieces(0)->AddRanges() =
            makeBlobRange(10_MB + 512_KB, 128_KB, 1_MB);

        *src.AddBlobPieces() = makeBlobPiece(4, 5, 6, 20);
        *src.MutableBlobPieces(1)->AddRanges() =
            makeBlobRange(10_MB + 256_KB, 64_KB, 0);
        *src.MutableBlobPieces(1)->AddRanges() =
            makeBlobRange(10_MB + 768_KB, 1_MB, 64_KB);

        {
            NProtoPrivate::TDescribeDataResponse dst;
            TByteRange range(10_MB, 128_KB, 4_KB);
            FilterResult(range, src, &dst);

            UNIT_ASSERT_VALUES_EQUAL(100_MB, dst.GetFileSize());

            const auto& freshRanges = dst.GetFreshDataRanges();
            UNIT_ASSERT_VALUES_EQUAL(3, freshRanges.size());
            UNIT_ASSERT_VALUES_EQUAL(
                10_MB + 10_KB,
                freshRanges[0].GetOffset());
            UNIT_ASSERT_VALUES_EQUAL(
                makeContent(10_MB + 10_KB, 1_KB),
                freshRanges[0].GetContent());
            UNIT_ASSERT_VALUES_EQUAL(
                10_MB + 100_KB,
                freshRanges[1].GetOffset());
            UNIT_ASSERT_VALUES_EQUAL(
                makeContent(10_MB + 100_KB, 7_KB),
                freshRanges[1].GetContent());
            UNIT_ASSERT_VALUES_EQUAL(
                10_MB + 127_KB,
                freshRanges[2].GetOffset());
            UNIT_ASSERT_VALUES_EQUAL(
                makeContent(10_MB + 127_KB, 1_KB),
                freshRanges[2].GetContent());

            const auto& blobPieces = dst.GetBlobPieces();
            UNIT_ASSERT_VALUES_EQUAL(1, blobPieces.size());
            UNIT_ASSERT_VALUES_EQUAL(1, blobPieces[0].GetBlobId().GetRawX1());
            UNIT_ASSERT_VALUES_EQUAL(2, blobPieces[0].GetBlobId().GetRawX2());
            UNIT_ASSERT_VALUES_EQUAL(3, blobPieces[0].GetBlobId().GetRawX3());
            UNIT_ASSERT_VALUES_EQUAL(10, blobPieces[0].GetBSGroupId());
            UNIT_ASSERT_VALUES_EQUAL(1, blobPieces[0].RangesSize());
            UNIT_ASSERT_VALUES_EQUAL(
                10_MB,
                blobPieces[0].GetRanges(0).GetOffset());
            UNIT_ASSERT_VALUES_EQUAL(
                128_KB,
                blobPieces[0].GetRanges(0).GetLength());
            UNIT_ASSERT_VALUES_EQUAL(
                512_KB,
                blobPieces[0].GetRanges(0).GetBlobOffset());
        }

        {
            NProtoPrivate::TDescribeDataResponse dst;
            TByteRange range(10_MB + 128_KB, 128_KB, 4_KB);
            FilterResult(range, src, &dst);

            UNIT_ASSERT_VALUES_EQUAL(100_MB, dst.GetFileSize());

            const auto& freshRanges = dst.GetFreshDataRanges();
            UNIT_ASSERT_VALUES_EQUAL(1, freshRanges.size());
            UNIT_ASSERT_VALUES_EQUAL(
                10_MB + 128_KB,
                freshRanges[0].GetOffset());
            UNIT_ASSERT_VALUES_EQUAL(
                makeContent(10_MB + 128_KB, 2_KB),
                freshRanges[0].GetContent());

            const auto& blobPieces = dst.GetBlobPieces();
            UNIT_ASSERT_VALUES_EQUAL(1, blobPieces.size());
            UNIT_ASSERT_VALUES_EQUAL(1, blobPieces[0].GetBlobId().GetRawX1());
            UNIT_ASSERT_VALUES_EQUAL(2, blobPieces[0].GetBlobId().GetRawX2());
            UNIT_ASSERT_VALUES_EQUAL(3, blobPieces[0].GetBlobId().GetRawX3());
            UNIT_ASSERT_VALUES_EQUAL(10, blobPieces[0].GetBSGroupId());
            UNIT_ASSERT_VALUES_EQUAL(1, blobPieces[0].RangesSize());
            UNIT_ASSERT_VALUES_EQUAL(
                10_MB + 128_KB,
                blobPieces[0].GetRanges(0).GetOffset());
            UNIT_ASSERT_VALUES_EQUAL(
                128_KB,
                blobPieces[0].GetRanges(0).GetLength());
            UNIT_ASSERT_VALUES_EQUAL(
                640_KB,
                blobPieces[0].GetRanges(0).GetBlobOffset());
        }

        {
            NProtoPrivate::TDescribeDataResponse dst;
            TByteRange range(11_MB - 128_KB, 128_KB, 4_KB);
            FilterResult(range, src, &dst);

            UNIT_ASSERT_VALUES_EQUAL(100_MB, dst.GetFileSize());

            const auto& freshRanges = dst.GetFreshDataRanges();
            UNIT_ASSERT_VALUES_EQUAL(0, freshRanges.size());

            const auto& blobPieces = dst.GetBlobPieces();
            UNIT_ASSERT_VALUES_EQUAL(1, blobPieces.size());
            UNIT_ASSERT_VALUES_EQUAL(4, blobPieces[0].GetBlobId().GetRawX1());
            UNIT_ASSERT_VALUES_EQUAL(5, blobPieces[0].GetBlobId().GetRawX2());
            UNIT_ASSERT_VALUES_EQUAL(6, blobPieces[0].GetBlobId().GetRawX3());
            UNIT_ASSERT_VALUES_EQUAL(20, blobPieces[0].GetBSGroupId());
            UNIT_ASSERT_VALUES_EQUAL(1, blobPieces[0].RangesSize());
            UNIT_ASSERT_VALUES_EQUAL(
                11_MB - 128_KB,
                blobPieces[0].GetRanges(0).GetOffset());
            UNIT_ASSERT_VALUES_EQUAL(
                128_KB,
                blobPieces[0].GetRanges(0).GetLength());
            UNIT_ASSERT_VALUES_EQUAL(
                192_KB,
                blobPieces[0].GetRanges(0).GetBlobOffset());
        }

        {
            NProtoPrivate::TDescribeDataResponse dst;
            TByteRange range(11_MB - 256_KB, 256_KB, 4_KB);
            FilterResult(range, src, &dst);

            UNIT_ASSERT_VALUES_EQUAL(100_MB, dst.GetFileSize());

            const auto& freshRanges = dst.GetFreshDataRanges();
            UNIT_ASSERT_VALUES_EQUAL(0, freshRanges.size());

            const auto& blobPieces = dst.GetBlobPieces();
            UNIT_ASSERT_VALUES_EQUAL(1, blobPieces.size());
            UNIT_ASSERT_VALUES_EQUAL(4, blobPieces[0].GetBlobId().GetRawX1());
            UNIT_ASSERT_VALUES_EQUAL(5, blobPieces[0].GetBlobId().GetRawX2());
            UNIT_ASSERT_VALUES_EQUAL(6, blobPieces[0].GetBlobId().GetRawX3());
            UNIT_ASSERT_VALUES_EQUAL(20, blobPieces[0].GetBSGroupId());
            UNIT_ASSERT_VALUES_EQUAL(1, blobPieces[0].RangesSize());
            UNIT_ASSERT_VALUES_EQUAL(
                11_MB - 256_KB,
                blobPieces[0].GetRanges(0).GetOffset());
            UNIT_ASSERT_VALUES_EQUAL(
                256_KB,
                blobPieces[0].GetRanges(0).GetLength());
            UNIT_ASSERT_VALUES_EQUAL(
                64_KB,
                blobPieces[0].GetRanges(0).GetBlobOffset());
        }
    }
}

}   // namespace NCloud::NFileStore::NStorage
