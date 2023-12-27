#pragma once

#include "datashard_impl.h"
#include "datashard_locks.h"
#include "datashard__engine_host.h"
#include "operation.h"

#include <contrib/ydb/core/tx/tx_processing.h>
#include <contrib/ydb/core/tablet_flat/flat_cxx_database.h>

#include <contrib/ydb/library/yql/public/issue/yql_issue.h>

namespace NKikimr {
namespace NDataShard {


class TValidatedWriteTx: TNonCopyable {
public:
    using TPtr = std::shared_ptr<TValidatedWriteTx>;

    TValidatedWriteTx(TDataShard* self, TTransactionContext& txc, const TActorContext& ctx, const TStepOrder& stepTxId, TInstant receivedAt, const NEvents::TDataEvents::TEvWrite::TPtr& ev);

    ~TValidatedWriteTx();

    static constexpr ui64 MaxReorderTxKeys() {
        return 100;
    }

    NKikimrTxDataShard::TError::EKind Code() const {
        return ErrCode;
    }
    const TString GetError() const {
        return ErrStr;
    }

    const NEvents::TDataEvents::TEvWrite::TPtr& GetEv() const {
        return Ev;
    }

    const NKikimrDataEvents::TEvWrite& GetRecord() const {
        return Ev->Get()->Record;
    }

    const NKikimrDataEvents::TEvWrite::TOperation& RecordOperation() const {
        Y_ABORT_UNLESS(GetRecord().operations().size() == 1, "Only one operation is supported now");
        return GetRecord().operations(0);
    }

    ui64 LockTxId() const {
        return GetRecord().locktxid();
    }
    ui32 LockNodeId() const {
        return GetRecord().locknodeid();
    }
    bool Immediate() const {
        return GetRecord().txmode() == NKikimrDataEvents::TEvWrite::MODE_IMMEDIATE;
    }
    bool NeedDiagnostics() const {
        return true;
    }
    bool CollectStats() const {
        return true;
    }
    bool Ready() const {
        return ErrCode == NKikimrTxDataShard::TError::OK;
    }
    bool RequirePrepare() const {
        return ErrCode == NKikimrTxDataShard::TError::SNAPSHOT_NOT_READY_YET;
    }
    bool RequireWrites() const {
        return TxInfo().HasWrites() || !Immediate();
    }
    bool HasWrites() const {
        return TxInfo().HasWrites();
    }
    bool HasLockedWrites() const {
        return HasWrites() && LockTxId();
    }
    bool HasDynamicWrites() const {
        return TxInfo().DynKeysCount != 0;
    }

    // TODO: It's an expensive operation (Precharge() inside). We need avoid it.
    TEngineBay::TSizes CalcReadSizes(bool needsTotalKeysSize) const {
        return EngineBay.CalcSizes(needsTotalKeysSize);
    }

    ui64 GetMemoryAllocated() const {
        return EngineBay.GetEngine() ? EngineBay.GetEngine()->GetMemoryAllocated() : 0;
    }

    NMiniKQL::IEngineFlat* GetEngine() {
        return EngineBay.GetEngine();
    }
    void DestroyEngine() {
        EngineBay.DestroyEngine();
    }
    const NMiniKQL::TEngineHostCounters& GetCounters() {
        return EngineBay.GetCounters();
    }
    void ResetCounters() {
        EngineBay.ResetCounters();
    }

    bool CanCancel();
    bool CheckCancelled();

    void SetWriteVersion(TRowVersion writeVersion) {
        EngineBay.SetWriteVersion(writeVersion);
    }
    void SetReadVersion(TRowVersion readVersion) {
        EngineBay.SetReadVersion(readVersion);
    }
    void SetVolatileTxId(ui64 txId) {
        EngineBay.SetVolatileTxId(txId);
    }

    void CommitChanges(const TTableId& tableId, ui64 lockId, const TRowVersion& writeVersion) {
        EngineBay.CommitChanges(tableId, lockId, writeVersion);
    }

    TVector<IDataShardChangeCollector::TChange> GetCollectedChanges() const {
        return EngineBay.GetCollectedChanges();
    }
    void ResetCollectedChanges() {
        EngineBay.ResetCollectedChanges();
    }

    TVector<ui64> GetVolatileCommitTxIds() const {
        return EngineBay.GetVolatileCommitTxIds();
    }
    const absl::flat_hash_set<ui64>& GetVolatileDependencies() const {
        return EngineBay.GetVolatileDependencies();
    }
    std::optional<ui64> GetVolatileChangeGroup() const {
        return EngineBay.GetVolatileChangeGroup();
    }
    bool GetVolatileCommitOrdered() const {
        return EngineBay.GetVolatileCommitOrdered();
    }

    bool IsProposed() const {
        return Source != TActorId();
    }

    inline const ::NKikimrDataEvents::TKqpLocks& GetKqpLocks() const {
        return GetRecord().locks();
    }

    bool ParseRecord(const TDataShard::TTableInfos& tableInfos);
    void SetTxKeys(const ::google::protobuf::RepeatedField<::NProtoBuf::uint32>& columnIds, const NScheme::TTypeRegistry& typeRegistry, ui64 tabletId, const TActorContext& ctx);

    ui32 ExtractKeys(bool allowErrors);
    bool ReValidateKeys();

    ui32 KeysCount() const {
        return TxInfo().WritesCount;
    }

    void ReleaseTxData();

    bool IsTxInfoLoaded() const {
        return TxInfo().Loaded;
    }

    bool HasOutReadsets() const {
        return TxInfo().HasOutReadsets;
    }
    bool HasInReadsets() const {
        return TxInfo().HasInReadsets;
    }

    const NMiniKQL::IEngineFlat::TValidationInfo& TxInfo() const {
        return EngineBay.TxInfo();
    }

private:
    const NEvents::TDataEvents::TEvWrite::TPtr& Ev;
    TEngineBay EngineBay;

    YDB_ACCESSOR_DEF(TActorId, Source);

    YDB_READONLY(TStepOrder, StepTxId, TStepOrder(0, 0));
    YDB_READONLY_DEF(TTableId, TableId);
    YDB_READONLY_DEF(TSerializedCellMatrix, Matrix);
    YDB_READONLY_DEF(TInstant, ReceivedAt);

    YDB_READONLY_DEF(ui64, TxSize);

    YDB_READONLY_DEF(NKikimrTxDataShard::TError::EKind, ErrCode);
    YDB_READONLY_DEF(TString, ErrStr);
    YDB_READONLY_DEF(bool, IsReleased);

    const TUserTable* TableInfo;
private:
    void ComputeTxSize();
};

class TWriteOperation : public TOperation {
    friend class TWriteUnit;
public:
    explicit TWriteOperation(const TBasicOpInfo& op, NEvents::TDataEvents::TEvWrite::TPtr ev, TDataShard* self, TTransactionContext& txc, const TActorContext& ctx);

    ~TWriteOperation();

    void FillTxData(TValidatedWriteTx::TPtr dataTx);
    void FillTxData(TDataShard* self, TTransactionContext& txc, const TActorContext& ctx, const TActorId& target, NEvents::TDataEvents::TEvWrite::TPtr&& ev, const TVector<TSysTables::TLocksTable::TLock>& locks, ui64 artifactFlags);
    void FillVolatileTxData(TDataShard* self, TTransactionContext& txc, const TActorContext& ctx);

    const NEvents::TDataEvents::TEvWrite::TPtr& GetEv() const {
        return Ev;
    }
    void SetEv(const NEvents::TDataEvents::TEvWrite::TPtr& ev) {
        UntrackMemory();
        Ev = ev;
        TrackMemory();
    }
    void ClearEv() {
        UntrackMemory();
        Ev.Reset();
        TrackMemory();
    }

    void Deactivate() override {
        ClearEv();

        TOperation::Deactivate();
    }

    ui32 ExtractKeys() {
        return WriteTx ? WriteTx->ExtractKeys(false) : 0;
    }

    bool ReValidateKeys() {
        return WriteTx ? WriteTx->ReValidateKeys() : true;
    }

    void MarkAsUsingSnapshot() {
        SetUsingSnapshotFlag();
    }

    bool IsTxDataReleased() const {
        return ReleasedTxDataSize > 0;
    }

    enum EArtifactFlags {
        OUT_RS_STORED = (1 << 0),
        LOCKS_STORED = (1 << 1),
    };
    void MarkOutRSStored() {
        ArtifactFlags |= OUT_RS_STORED;
    }

    bool IsOutRSStored() {
        return ArtifactFlags & OUT_RS_STORED;
    }

    void MarkLocksStored() {
        ArtifactFlags |= LOCKS_STORED;
    }

    bool IsLocksStored() {
        return ArtifactFlags & LOCKS_STORED;
    }

    void DbStoreLocksAccessLog(ui64 tabletId, TTransactionContext& txc, const TActorContext& ctx);
    void DbStoreArtifactFlags(ui64 tabletId, TTransactionContext& txc, const TActorContext& ctx);

    ui64 GetMemoryConsumption() const;

    ui64 GetRequiredMemory() const {
        Y_ABORT_UNLESS(!GetTxCacheUsage() || !IsTxDataReleased());
        ui64 requiredMem = GetTxCacheUsage() + GetReleasedTxDataSize();
        if (!requiredMem)
            requiredMem = GetMemoryConsumption();
        return requiredMem;
    }

    void ReleaseTxData(NTabletFlatExecutor::TTxMemoryProviderBase& provider, const TActorContext& ctx);
    ERestoreDataStatus RestoreTxData(TDataShard* self, TTransactionContext& txc, const TActorContext& ctx);
    void FinalizeWriteTxPlan();

    // TOperation iface.
    void BuildExecutionPlan(bool loaded) override;

    bool HasKeysInfo() const override {
        return WriteTx ? WriteTx->TxInfo().Loaded : false;
    }

    const NMiniKQL::IEngineFlat::TValidationInfo& GetKeysInfo() const override {
        if (WriteTx) {
            Y_ABORT_UNLESS(WriteTx->TxInfo().Loaded);
            return WriteTx->TxInfo();
        }
        // For scheme tx global reader and writer flags should
        // result in all required dependencies.
        return TOperation::GetKeysInfo();
    }

    ui64 LockTxId() const override {
        return WriteTx ? WriteTx->LockTxId() : 0;
    }

    ui32 LockNodeId() const override {
        return WriteTx ? WriteTx->LockNodeId() : 0;
    }

    bool HasLockedWrites() const override {
        return WriteTx ? WriteTx->HasLockedWrites() : false;
    }

    ui64 IncrementPageFaultCount() {
        return ++PageFaultCount;
    }

    const TValidatedWriteTx::TPtr& GetWriteTx() const { 
        return WriteTx; 
    }
    TValidatedWriteTx::TPtr BuildWriteTx(TDataShard* self, TTransactionContext& txc, const TActorContext& ctx);

    void ClearWriteTx() { 
        WriteTx = nullptr; 
    }

    const NKikimrDataEvents::TEvWrite& GetRecord() const {
        return Ev->Get()->Record;
    }

    const std::unique_ptr<NEvents::TDataEvents::TEvWriteResult>& GetWriteResult() const {
        return WriteResult;
    }
    std::unique_ptr<NEvents::TDataEvents::TEvWriteResult>&& ReleaseWriteResult() {
        return std::move(WriteResult);
    }

    void SetError(const NKikimrDataEvents::TEvWriteResult::EStatus& status, const TString& errorMsg, ui64 tabletId);
    void SetWriteResult(std::unique_ptr<NEvents::TDataEvents::TEvWriteResult>&& writeResult);

private:
    void TrackMemory() const;
    void UntrackMemory() const;

private:
    NEvents::TDataEvents::TEvWrite::TPtr Ev;
    TValidatedWriteTx::TPtr WriteTx;
    std::unique_ptr<NEvents::TDataEvents::TEvWriteResult> WriteResult;

    YDB_READONLY_DEF(ui64, ArtifactFlags);
    YDB_ACCESSOR_DEF(ui64, TxCacheUsage);
    YDB_ACCESSOR_DEF(ui64, ReleasedTxDataSize);
    YDB_ACCESSOR_DEF(ui64, SchemeShardId);
    YDB_ACCESSOR_DEF(ui64, SubDomainPathId);
    YDB_ACCESSOR_DEF(NKikimrSubDomains::TProcessingParams, ProcessingParams);
    
    ui64 PageFaultCount = 0;
};

} // NDataShard
} // NKikimr
