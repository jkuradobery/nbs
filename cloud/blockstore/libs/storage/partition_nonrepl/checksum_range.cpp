#include "checksum_range.h"

#include <cloud/blockstore/libs/storage/core/probes.h>
#include <cloud/blockstore/libs/storage/disk_agent/public.h>

#include <contrib/ydb/library/actors/core/log.h>

namespace NCloud::NBlockStore::NStorage {

using namespace NActors;

////////////////////////////////////////////////////////////////////////////////

TChecksumRangeActorCompanion::TChecksumRangeActorCompanion(
        TVector<TReplicaDescriptor> replicas)
    : Replicas(std::move(replicas))
{
    Checksums.resize(Replicas.size());
}

bool TChecksumRangeActorCompanion::IsFinished() const
{
    return CalculatedChecksumsCount == Replicas.size();
}

const TVector<ui64>& TChecksumRangeActorCompanion::GetChecksums() const
{
    return Checksums;
}

NProto::TError TChecksumRangeActorCompanion::GetError() const
{
    return Error;
}

TInstant TChecksumRangeActorCompanion::GetChecksumStartTs() const
{
    return ChecksumStartTs;
}

TDuration TChecksumRangeActorCompanion::GetChecksumDuration() const
{
    return ChecksumDuration;
}

void TChecksumRangeActorCompanion::CalculateChecksums(
    const TActorContext& ctx,
    TBlockRange64 range)
{
    for (size_t i = 0; i < Replicas.size(); ++i) {
        CalculateReplicaChecksum(ctx, range, i);
    }
    ChecksumStartTs = ctx.Now();
}

void TChecksumRangeActorCompanion::CalculateReplicaChecksum(
    const TActorContext& ctx,
    TBlockRange64 range,
    int idx)
{
    auto request = std::make_unique<TEvNonreplPartitionPrivate::TEvChecksumBlocksRequest>();
    request->Record.SetStartIndex(range.Start);
    request->Record.SetBlocksCount(range.Size());

    auto* headers = request->Record.MutableHeaders();
    headers->SetIsBackgroundRequest(true);
    headers->SetClientId(TString(BackgroundOpsClientId));

    auto event = std::make_unique<NActors::IEventHandle>(
        Replicas[idx].ActorId,
        ctx.SelfID,
        request.release(),
        IEventHandle::FlagForwardOnNondelivery,
        idx,          // cookie
        &ctx.SelfID   // forwardOnNondelivery
    );

    ctx.Send(event.release());
}

////////////////////////////////////////////////////////////////////////////////

void TChecksumRangeActorCompanion::HandleChecksumResponse(
    const TEvNonreplPartitionPrivate::TEvChecksumBlocksResponse::TPtr& ev,
    const TActorContext& ctx)
{
    ++CalculatedChecksumsCount;
    auto* msg = ev->Get();
    if (HasError(msg->Record.GetError())) {
        LOG_WARN(ctx, TBlockStoreComponents::PARTITION,
            "[%s] Checksum error %s",
            Replicas[0].Name.c_str(),
            FormatError(Error).c_str());

        Error = msg->Record.GetError();
        ChecksumDuration = ctx.Now() - ChecksumStartTs;
        return;
    }

    Checksums[ev->Cookie] = msg->Record.GetChecksum();
    if (CalculatedChecksumsCount == Replicas.size()) {
        ChecksumDuration = ctx.Now() - ChecksumStartTs;
    }
}

void TChecksumRangeActorCompanion::HandleChecksumUndelivery(
    const NActors::TActorContext& ctx)
{
    ++CalculatedChecksumsCount;
    ChecksumDuration = ctx.Now() - ChecksumStartTs;
    Error = MakeError(E_REJECTED, "ChecksumBlocks request undelivered");
}

}   // namespace NCloud::NBlockStore::NStorage
