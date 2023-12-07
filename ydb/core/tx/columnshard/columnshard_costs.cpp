#include <ydb/core/tx/columnshard/columnshard_costs.h>
#include <ydb/core/tx/columnshard/engines/index_info.h>
#include <ydb/core/tx/columnshard/engines/granules_table.h>
#include <ydb/core/formats/arrow_helpers.h>
#include <ydb/core/scheme/scheme_tabledefs.h>
#include <ydb/core/protos/ssa.pb.h>
#include <ydb/core/tx/columnshard/engines/predicate.h>

namespace NKikimr::NOlap::NCosts {

void TKeyRangesBuilder::AddMarkFromGranule(const TGranuleRecord& record) {
    Constructor.StartRecord(true).AddRecordValue(record.Mark);
    Features.emplace_back(TMarkRangeFeatures());
}

bool TKeyRangesBuilder::AddMarkFromPredicate(const std::shared_ptr<NOlap::TPredicate>& p) {
    if (!p) {
        return false;
    }
    Y_VERIFY(p->Batch->num_rows() == 1);
    Constructor.AddRecordsBatchSlow(p->Batch);
    Features.emplace_back(TMarkRangeFeatures().SetMarkIncluded(p->Inclusive));
    return true;
}

void TKeyRangesBuilder::FillRangeMarks(const std::shared_ptr<NOlap::TPredicate>& left, const TVector<TGranuleRecord>& granuleRecords,
    const std::shared_ptr<NOlap::TPredicate>& right) {
    LOG_S_DEBUG("TCostsOperator::BuildRangeMarks::Request from " << (left ? left->Batch->ToString() : "-Inf") <<
        " to " << (right ? right->Batch->ToString() : "+Inf"));

    if (!AddMarkFromPredicate(left)) {
        Y_VERIFY(!LeftBorderOpened);
        LeftBorderOpened = true;
    }
    for (auto&& rec : granuleRecords) {
        AddMarkFromGranule(rec);
    }
    if (AddMarkFromPredicate(right)) {
        Features.back().SetIntervalSkipped(true);
    }
}

NKikimrKqp::TEvRemoteCostData::TCostInfo TKeyRanges::SerializeToProto() const {
    NKikimrKqp::TEvRemoteCostData::TCostInfo result;
    result.SetLeftBorderOpened(LeftBorderOpened);
    for (auto&& i : RangeMarkFeatures) {
        *result.AddIntervalMeta() = i.SerializeToProto();
    }
    BatchReader.SerializeToStrings(*result.MutableColumnsSchema(), *result.MutableColumnsData());
    return result;
}

bool TKeyRanges::DeserializeFromProto(const NKikimrKqp::TEvRemoteCostData::TCostInfo& proto) {
    Y_VERIFY(BatchReader.DeserializeFromStrings(proto.GetColumnsSchema(), proto.GetColumnsData()));
    LeftBorderOpened = proto.GetLeftBorderOpened();
    Y_VERIFY(BatchReader.GetRowsCount() == (size_t)proto.GetIntervalMeta().size());
    RangeMarkFeatures.reserve(proto.GetIntervalMeta().size());
    for (auto&& i : proto.GetIntervalMeta()) {
        TMarkRangeFeatures features;
        Y_VERIFY(features.DeserializeFromProto(i));
        RangeMarkFeatures.emplace_back(std::move(features));
    }
    return true;
}

TKeyRangesBuilder::TKeyRangesBuilder(const TIndexInfo& indexInfo)
    : IndexInfo(indexInfo)
{
    Constructor.InitColumns(NArrow::MakeArrowSchema(IndexInfo.GetPK()));
}

NKikimr::NOlap::NCosts::TKeyRanges TKeyRangesBuilder::Build() {
    TKeyRanges result;
    result.LeftBorderOpened = LeftBorderOpened;
    result.RangeMarkFeatures = std::move(Features);
    result.BatchReader = Constructor.Finish();
    return result;
}

}
