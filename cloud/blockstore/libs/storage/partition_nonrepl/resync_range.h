#pragma once

#include "part_nonrepl_events_private.h"

#include <cloud/blockstore/libs/diagnostics/profile_log.h>
#include <cloud/blockstore/libs/diagnostics/public.h>
#include <cloud/blockstore/libs/storage/api/service.h>
#include <cloud/blockstore/libs/storage/core/request_info.h>

#include <cloud/storage/core/libs/common/error.h>

#include <ydb/library/actors/core/actorid.h>
#include <ydb/library/actors/core/actor_bootstrapped.h>
#include <ydb/library/actors/core/events.h>

namespace NCloud::NBlockStore::NStorage {

////////////////////////////////////////////////////////////////////////////////

struct TResyncReplica
{
    TString Name;
    NActors::TActorId ActorId;
};

////////////////////////////////////////////////////////////////////////////////

class TResyncRangeActor final
    : public NActors::TActorBootstrapped<TResyncRangeActor>
{
private:
    const TRequestInfoPtr RequestInfo;
    const ui32 BlockSize;
    const TBlockRange64 Range;
    const TVector<TResyncReplica> Replicas;
    const TString WriterClientId;
    const IBlockDigestGeneratorPtr BlockDigestGenerator;

    THashMap<int, ui64> Checksums;
    TVector<int> ActorsToResync;
    ui32 ResyncedCount = 0;
    TGuardedBuffer<TString> Buffer;
    TGuardedSgList SgList;
    NProto::TError Error;

    TInstant ChecksumStartTs;
    TDuration ChecksumDuration;
    TInstant ReadStartTs;
    TDuration ReadDuration;
    TInstant WriteStartTs;
    TDuration WriteDuration;
    TVector<IProfileLog::TBlockInfo> AffectedBlockInfos;

public:
    TResyncRangeActor(
        TRequestInfoPtr requestInfo,
        ui32 blockSize,
        TBlockRange64 range,
        TVector<TResyncReplica> replicas,
        TString writerClientId,
        IBlockDigestGeneratorPtr blockDigestGenerator);

    void Bootstrap(const NActors::TActorContext& ctx);

private:
    void ChecksumBlocks(const NActors::TActorContext& ctx);
    void ChecksumReplicaBlocks(const NActors::TActorContext& ctx, int idx);
    void CompareChecksums(const NActors::TActorContext& ctx);
    void ReadBlocks(const NActors::TActorContext& ctx, int idx);
    void WriteBlocks(const NActors::TActorContext& ctx);
    void WriteReplicaBlocks(const NActors::TActorContext& ctx, int idx);
    void Done(const NActors::TActorContext& ctx);

private:
    STFUNC(StateWork);

    void HandleChecksumResponse(
        const TEvNonreplPartitionPrivate::TEvChecksumBlocksResponse::TPtr& ev,
        const NActors::TActorContext& ctx);

    void HandleChecksumUndelivery(
        const TEvNonreplPartitionPrivate::TEvChecksumBlocksRequest::TPtr& ev,
        const NActors::TActorContext& ctx);

    void HandleReadResponse(
        const TEvService::TEvReadBlocksLocalResponse::TPtr& ev,
        const NActors::TActorContext& ctx);

    void HandleReadUndelivery(
        const TEvService::TEvReadBlocksLocalRequest::TPtr& ev,
        const NActors::TActorContext& ctx);

    void HandleWriteResponse(
        const TEvService::TEvWriteBlocksLocalResponse::TPtr& ev,
        const NActors::TActorContext& ctx);

    void HandleWriteUndelivery(
        const TEvService::TEvWriteBlocksLocalRequest::TPtr& ev,
        const NActors::TActorContext& ctx);

    void HandlePoisonPill(
        const NActors::TEvents::TEvPoisonPill::TPtr& ev,
        const NActors::TActorContext& ctx);
};

}   // namespace NCloud::NBlockStore::NStorage
