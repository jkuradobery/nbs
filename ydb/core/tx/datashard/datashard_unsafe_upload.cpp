#include "datashard_txs.h"

namespace NKikimr {
namespace NDataShard {

TDataShard::TTxUnsafeUploadRows::TTxUnsafeUploadRows(TDataShard* ds, TEvDataShard::TEvUnsafeUploadRowsRequest::TPtr& ev)
    : TBase(ds)
    , TCommonUploadOps(ev, false, false)
{
}

bool TDataShard::TTxUnsafeUploadRows::Execute(TTransactionContext& txc, const TActorContext&) {
    auto [readVersion, writeVersion] = Self->GetReadWriteVersions();
    if (!TCommonUploadOps::Execute(Self, txc, readVersion, writeVersion)) {
        return false;
    }

    auto* result = GetResult();
    if (result->Record.GetStatus() == NKikimrTxDataShard::TError::OK) {
        NIceDb::TNiceDb db(txc.DB);
        result->Info = Self->S3Downloads.Store(db, GetRequest()->TxId, GetRequest()->Info);
    }

    if (Self->IsMvccEnabled()) {
        // Note: we always wait for completion, so we can ignore the result
        Self->PromoteImmediatePostExecuteEdges(writeVersion, TDataShard::EPromotePostExecuteEdges::ReadWrite, txc);
        MvccVersion = writeVersion;
    }

    return true;
}

void TDataShard::TTxUnsafeUploadRows::Complete(const TActorContext& ctx) {
    TActorId target;
    THolder<IEventBase> event;
    ui64 cookie;
    TCommonUploadOps::GetResult(Self, target, event, cookie);

    if (MvccVersion) {
        Self->SendImmediateWriteResult(MvccVersion, target, event.Release(), cookie);
    } else {
        ctx.Send(target, event.Release(), 0, cookie);
    }
}

} // NDataShard
} // NKikimr
