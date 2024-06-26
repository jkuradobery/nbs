#pragma once
#include "common.h"
#include <util/generic/hash_set.h>
#include <contrib/ydb/core/tx/columnshard/blob.h>
#include <contrib/ydb/library/accessor/accessor.h>
#include <contrib/ydb/core/tx/columnshard/blobs_action/counters/remove_declare.h>

namespace NKikimr::NColumnShard {
class TColumnShard;
class TBlobManagerDb;
}

namespace NKikimr::NOlap {

class IBlobsDeclareRemovingAction: public ICommonBlobsAction {
private:
    using TBase = ICommonBlobsAction;
    std::shared_ptr<NBlobOperations::TRemoveDeclareCounters> Counters;
    YDB_READONLY_DEF(THashSet<TUnifiedBlobId>, DeclaredBlobs);
protected:
    virtual void DoDeclareRemove(const TUnifiedBlobId& blobId) = 0;
    virtual void DoOnExecuteTxAfterRemoving(NColumnShard::TColumnShard& self, NColumnShard::TBlobManagerDb& dbBlobs, const bool success) = 0;
    virtual void DoOnCompleteTxAfterRemoving(NColumnShard::TColumnShard& self) = 0;
public:
    IBlobsDeclareRemovingAction(const TString& storageId)
        : TBase(storageId)
    {

    }

    void SetCounters(const std::shared_ptr<NBlobOperations::TRemoveDeclareCounters>& counters) {
        Counters = counters;
    }

    void DeclareRemove(const TUnifiedBlobId& blobId);
    void OnExecuteTxAfterRemoving(NColumnShard::TColumnShard& self, NColumnShard::TBlobManagerDb& dbBlobs, const bool success) {
        return DoOnExecuteTxAfterRemoving(self, dbBlobs, success);
    }
    void OnCompleteTxAfterRemoving(NColumnShard::TColumnShard& self) {
        return DoOnCompleteTxAfterRemoving(self);
    }
};

}
