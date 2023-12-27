#include "indexed_blob_constructor.h"

#include <contrib/ydb/core/tx/columnshard/defs.h>
#include <contrib/ydb/core/tx/columnshard/blob.h>
#include <contrib/ydb/core/tx/columnshard/columnshard_private_events.h>


namespace NKikimr::NOlap {

TIndexedWriteController::TIndexedWriteController(const TActorId& dstActor, const std::shared_ptr<IBlobsWritingAction>& action, std::vector<std::shared_ptr<TWriteAggregation>>&& aggregations)
    : Buffer(action, std::move(aggregations))
    , DstActor(dstActor)
{
    auto blobs = Buffer.GroupIntoBlobs();
    for (auto&& b : blobs) {
        auto& task = AddWriteTask(TBlobWriteInfo::BuildWriteTask(b.GetBlobData(), action));
        b.InitBlobId(task.GetBlobId());
    }
}

void TIndexedWriteController::DoOnReadyResult(const NActors::TActorContext& ctx, const NColumnShard::TBlobPutResult::TPtr& putResult) {
    Buffer.InitReadyInstant(TMonotonic::Now());
    auto result = std::make_unique<NColumnShard::TEvPrivate::TEvWriteBlobsResult>(putResult, std::move(Buffer));
    ctx.Send(DstActor, result.release());
}

void TIndexedWriteController::DoOnStartSending() {
    Buffer.InitStartSending(TMonotonic::Now());
}

void TWideSerializedBatch::InitBlobId(const TUnifiedBlobId& id) {
    AFL_VERIFY(!Range.BlobId.GetTabletId());
    Range.BlobId = id;
}

void TWritingBuffer::InitReadyInstant(const TMonotonic instant) {
    for (auto&& aggr : Aggregations) {
        aggr->GetWriteData()->MutableWriteMeta().SetWriteMiddle4StartInstant(instant);
    }
}

void TWritingBuffer::InitStartSending(const TMonotonic instant) {
    for (auto&& aggr : Aggregations) {
        aggr->GetWriteData()->MutableWriteMeta().SetWriteMiddle5StartInstant(instant);
    }
}

}
