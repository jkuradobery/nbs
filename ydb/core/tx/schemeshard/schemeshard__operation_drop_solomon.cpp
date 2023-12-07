#include "schemeshard__operation_part.h"
#include "schemeshard__operation_common.h"
#include "schemeshard_impl.h"

#include <ydb/core/base/subdomain.h>

namespace {

using namespace NKikimr;
using namespace NSchemeShard;

class TDeleteParts: public TSubOperationState {
private:
    TOperationId OperationId;

    TString DebugHint() const override {
        return TStringBuilder()
                << "TDropSolomon TDeleteParts"
                << ", operationId: " << OperationId;
    }

public:
    TDeleteParts(TOperationId id)
        : OperationId(id)
    {
        IgnoreMessages(DebugHint(), {TEvPrivate::TEvOperationPlan::EventType});
    }

    bool ProgressState(TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ", at schemeshard: " << ssId);

        TTxState* txState = context.SS->FindTx(OperationId);

        // Initiate asynchronous deletion of all shards
        for (auto shard : txState->Shards) {
            context.OnComplete.DeleteShard(shard.Idx);
        }

        context.OnComplete.DoneOperation(OperationId);
        return true;
    }
};

class TPropose: public TSubOperationState {
private:
    TOperationId OperationId;

    TString DebugHint() const override {
        return TStringBuilder()
                << "TDropSolomon TPropose"
                << " operationId#" << OperationId;
    }

public:
    TPropose(TOperationId id)
        : OperationId(id)
    {
        IgnoreMessages(DebugHint(), {});
    }

    bool HandleReply(TEvPrivate::TEvOperationPlan::TPtr& ev, TOperationContext& context) override {
        TStepId step = TStepId(ev->Get()->StepId);
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " HandleReply TEvOperationPlan"
                               << ", step: " << step
                               << ", at schemeshard: " << ssId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState->TxType == TTxState::TxDropSolomonVolume);


        TPathId pathId = txState->TargetPathId;
        TPathElement::TPtr path = context.SS->PathsById.at(pathId);

        NIceDb::TNiceDb db(context.GetDB());

        auto paths = context.SS->ListSubTree(pathId, context.Ctx);
        Y_VERIFY(paths.size() == 1);
        context.SS->DropPaths(paths, step, OperationId.GetTxId(), db, context.Ctx);

        if (!AppData()->DisableSchemeShardCleanupOnDropForTest) {
            context.SS->PersistRemoveSolomonVolume(db, pathId);
        }

        auto parentDir = context.SS->PathsById.at(path->ParentPathId);
        ++parentDir->DirAlterVersion;
        context.SS->PersistPathDirAlterVersion(db, parentDir);
        context.SS->ClearDescribePathCaches(parentDir);
        context.SS->ClearDescribePathCaches(path);

        if (!context.SS->DisablePublicationsOfDropping) {
            context.OnComplete.PublishToSchemeBoard(OperationId, parentDir->PathId);
            context.OnComplete.PublishToSchemeBoard(OperationId, pathId);
        }

        context.SS->ChangeTxState(db, OperationId, TTxState::ProposedDeleteParts);
        return true;
    }

    bool ProgressState(TOperationContext& context) override {
        TTabletId ssId = context.SS->SelfTabletId();

        LOG_INFO_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                   DebugHint() << " ProgressState"
                               << ",  at schemeshard: " << ssId);

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);
        Y_VERIFY(txState->TxType == TTxState::TxDropSolomonVolume);

        context.OnComplete.ProposeToCoordinator(OperationId, txState->TargetPathId, TStepId(0));
        return false;
    }
};

class TDropSolomon: public TSubOperation {
    static TTxState::ETxState NextState() {
        return TTxState::Propose;
    }

    TTxState::ETxState NextState(TTxState::ETxState state) const override {
        switch (state) {
        case TTxState::Waiting:
        case TTxState::Propose:
            return TTxState::ProposedDeleteParts;
        default:
            return TTxState::Invalid;
        }
    }

    TSubOperationState::TPtr SelectStateFunc(TTxState::ETxState state) override {
        switch (state) {
        case TTxState::Waiting:
        case TTxState::Propose:
            return MakeHolder<TPropose>(OperationId);
        case TTxState::ProposedDeleteParts:
            return MakeHolder<TDeleteParts>(OperationId);
        default:
            return nullptr;
        }
    }

public:
    using TSubOperation::TSubOperation;

    THolder<TProposeResponse> Propose(const TString&, TOperationContext& context) override {
        const TTabletId ssId = context.SS->SelfTabletId();

        const auto& drop = Transaction.GetDrop();

        const TString& parentPathStr = Transaction.GetWorkingDir();
        const TString& name = drop.GetName();

        LOG_NOTICE_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                     "TDropSolomon Propose"
                         << ", path: " << parentPathStr << "/" << name
                         << ", pathId: " << drop.GetId()
                         << ", opId: " << OperationId
                         << ", at schemeshard: " << ssId);

        auto result = MakeHolder<TProposeResponse>(NKikimrScheme::StatusAccepted, ui64(OperationId.GetTxId()), ui64(ssId));

        TPath path = drop.HasId()
            ? TPath::Init(context.SS->MakeLocalId(drop.GetId()), context.SS)
            : TPath::Resolve(parentPathStr, context.SS).Dive(name);

        {
            TPath::TChecker checks = path.Check();
            checks
                .NotEmpty()
                .NotUnderDomainUpgrade()
                .IsAtLocalSchemeShard()
                .IsResolved()
                .NotDeleted()
                .NotUnderDeleting()
                .IsSolomon()
                .NotUnderOperation()
                .IsCommonSensePath();

            if (!checks) {
                result->SetError(checks.GetStatus(), checks.GetError());
                if (path.IsResolved() && path.Base()->IsSolomon() && path.Base()->PlannedToDrop()) {
                    result->SetPathDropTxId(ui64(path.Base()->DropTxId));
                    result->SetPathId(path.Base()->PathId.LocalPathId);
                }
                return result;
            }
        }

        TString errStr;
        if (!context.SS->CheckApplyIf(Transaction, errStr)) {
            result->SetError(NKikimrScheme::StatusPreconditionFailed, errStr);
            return result;
        }

        TTxState& txState = context.SS->CreateTx(OperationId, TTxState::TxDropSolomonVolume, path.Base()->PathId);
        // Dirty hack: drop step must not be zero because 0 is treated as "hasn't been dropped"
        txState.MinStep = TStepId(1);
        txState.State = TTxState::Propose;

        auto solomon = context.SS->SolomonVolumes.at(path.Base()->PathId);

        NIceDb::TNiceDb db(context.GetDB());

        for (auto& part : solomon->Partitions) {
            auto shardIdx = part.first;

            txState.Shards.emplace_back(shardIdx, context.SS->ShardInfos.at(shardIdx).TabletType, txState.State);

            context.SS->ShardInfos[shardIdx].CurrentTxId = OperationId.GetTxId();
            context.SS->PersistShardTx(db, shardIdx, OperationId.GetTxId());
        }

        context.OnComplete.ActivateTx(OperationId);
        context.SS->PersistTxState(db, OperationId);

        path.Base()->PathState = TPathElement::EPathState::EPathStateDrop;
        path.Base()->DropTxId = OperationId.GetTxId();
        path.Base()->LastTxId = OperationId.GetTxId();

        context.SS->TabletCounters->Simple()[COUNTER_SOLOMON_VOLUME_COUNT].Sub(1);

        auto parentDir = path.Parent();
        ++parentDir.Base()->DirAlterVersion;
        context.SS->PersistPathDirAlterVersion(db, parentDir.Base());
        context.SS->ClearDescribePathCaches(parentDir.Base());
        context.SS->ClearDescribePathCaches(path.Base());

        if (!context.SS->DisablePublicationsOfDropping) {
            context.OnComplete.PublishToSchemeBoard(OperationId, parentDir.Base()->PathId);
            context.OnComplete.PublishToSchemeBoard(OperationId, path.Base()->PathId);
        }

        SetState(NextState());
        return result;
    }

    void AbortPropose(TOperationContext&) override {
        Y_FAIL("no AbortPropose for TDropSolomon");
    }

    void AbortUnsafe(TTxId forceDropTxId, TOperationContext& context) override {
        LOG_NOTICE_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                     "TDropSolomon AbortUnsafe"
                         << ", opId: " << OperationId
                         << ", forceDropId: " << forceDropTxId
                         << ", at schemeshard: " << context.SS->TabletID());

        TTxState* txState = context.SS->FindTx(OperationId);
        Y_VERIFY(txState);

        TPathId pathId = txState->TargetPathId;
        Y_VERIFY(context.SS->PathsById.contains(pathId));
        TPathElement::TPtr path = context.SS->PathsById.at(pathId);
        Y_VERIFY(path);

        if (path->Dropped()) {
            for (auto shard : txState->Shards) {
                context.OnComplete.DeleteShard(shard.Idx);
            }
        }

        context.OnComplete.DoneOperation(OperationId);
    }
};

}

namespace NKikimr::NSchemeShard {

ISubOperationBase::TPtr CreateDropSolomon(TOperationId id, const TTxTransaction& tx) {
    return MakeSubOperation<TDropSolomon>(id, tx);
}

ISubOperationBase::TPtr CreateDropSolomon(TOperationId id, TTxState::ETxState state) {
    Y_VERIFY(state != TTxState::Invalid);
    return MakeSubOperation<TDropSolomon>(id, state);
}

}
