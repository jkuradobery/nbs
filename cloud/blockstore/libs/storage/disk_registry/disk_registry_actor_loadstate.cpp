#include "disk_registry_actor.h"

#include <ydb/core/base/appdata.h>

#include <util/generic/algorithm.h>

namespace NCloud::NBlockStore::NStorage {

using namespace NActors;

using namespace NKikimr;
using namespace NKikimr::NTabletFlatExecutor;

namespace {

////////////////////////////////////////////////////////////////////////////////

bool AllSucceeded(std::initializer_list<bool> ls)
{
    auto identity = [] (bool x) {
        return x;
    };

    return std::all_of(std::begin(ls), std::end(ls), identity);
}

// TODO: Remove legacy compatibility in next release
void ProcessUserNotifications(
    const TActorContext& ctx,
    TDiskRegistryDatabase& db,
    TVector<TString>& errorNotifications,
    TVector<NProto::TUserNotification>& userNotifications)
{
    // Filter out unknown events for future version rollback compatibility
    std::erase_if(userNotifications, [] (const auto& notif) {
            return notif.GetEventCase()
                == NProto::TUserNotification::EventCase::EVENT_NOT_SET;
        });

    THashSet<TString> ids(
        errorNotifications.begin(),
        errorNotifications.end(),
        errorNotifications.size());

    auto isObsolete = [&ids, now = ctx.Now()] (const auto& notif) {
        if (notif.GetHasLegacyCopy()
            && !ids.contains(notif.GetDiskError().GetDiskId()))
        {
            return true;
        }

        TDuration age = now - TInstant::MicroSeconds(notif.GetTimestamp());
        // It seems it's feasible to hardcode the threshold in temporary code
        return age >= TDuration::Days(3);
    };

    for (const auto& notif: userNotifications) {
        if (isObsolete(notif)) {
            LOG_INFO(ctx, TBlockStoreComponents::DISK_REGISTRY,
                "Obsolete user notification deleted: %s",
                (TStringBuilder() << notif).c_str()
            );
            db.DeleteUserNotification(notif.GetSeqNo());
        }
    }
    std::erase_if(userNotifications, isObsolete);
}

}   // namespace

////////////////////////////////////////////////////////////////////////////////

bool TDiskRegistryActor::LoadState(
    TDiskRegistryDatabase& db,
    TDiskRegistryStateSnapshot& args)
{
    return AllSucceeded({
        db.ReadDiskRegistryConfig(args.Config),
        db.ReadDirtyDevices(args.DirtyDevices),
        db.ReadOldAgents(args.OldAgents),
        db.ReadAgents(args.Agents),
        db.ReadDisks(args.Disks),
        db.ReadPlacementGroups(args.PlacementGroups),
        db.ReadBrokenDisks(args.BrokenDisks),
        db.ReadDisksToReallocate(args.DisksToReallocate),
        db.ReadErrorNotifications(args.ErrorNotifications),
        db.ReadUserNotifications(args.UserNotifications),
        db.ReadDiskStateChanges(args.DiskStateChanges),
        db.ReadLastDiskStateSeqNo(args.LastDiskStateSeqNo),
        db.ReadWritableState(args.WritableState),
        db.ReadDisksToCleanup(args.DisksToCleanup),
        db.ReadOutdatedVolumeConfigs(args.OutdatedVolumeConfigs),
        db.ReadSuspendedDevices(args.SuspendedDevices),
        db.ReadAutomaticallyReplacedDevices(args.AutomaticallyReplacedDevices),
        db.ReadDiskRegistryAgentListParams(args.DiskRegistryAgentListParams),
    });
}

////////////////////////////////////////////////////////////////////////////////

bool TDiskRegistryActor::PrepareLoadState(
    const TActorContext& ctx,
    TTransactionContext& tx,
    TTxDiskRegistry::TLoadState& args)
{
    Y_UNUSED(ctx);

    TDiskRegistryDatabase db(tx.DB);

    return AllSucceeded({
        LoadState(db, args.Snapshot),
        db.ReadRestoreState(args.RestoreState),
        db.ReadLastBackupTs(args.LastBackupTime)});
}

void TDiskRegistryActor::ExecuteLoadState(
    const TActorContext& ctx,
    TTransactionContext& tx,
    TTxDiskRegistry::TLoadState& args)
{
    // Move OldAgents to Agents

    THashSet<TString> ids;
    for (const auto& agent: args.Snapshot.Agents) {
        ids.insert(agent.GetAgentId());
    }

    TDiskRegistryDatabase db(tx.DB);

    for (auto& agent: args.Snapshot.OldAgents) {
        if (!ids.insert(agent.GetAgentId()).second) {
            continue;
        }

        LOG_INFO(ctx, TBlockStoreComponents::DISK_REGISTRY,
            "Agent %s:%d moved to new table",
            agent.GetAgentId().c_str(),
            agent.GetNodeId());

        args.Snapshot.Agents.push_back(agent);

        db.UpdateAgent(agent);
    }

    ProcessUserNotifications(
        ctx,
        db,
        args.Snapshot.ErrorNotifications,
        args.Snapshot.UserNotifications);
}

void TDiskRegistryActor::InitializeState(TDiskRegistryStateSnapshot snapshot)
{
    State = std::make_unique<TDiskRegistryState>(
        Logging,
        Config,
        ComponentGroup,
        std::move(snapshot.Config),
        std::move(snapshot.Agents),
        std::move(snapshot.Disks),
        std::move(snapshot.PlacementGroups),
        std::move(snapshot.BrokenDisks),
        std::move(snapshot.DisksToReallocate),
        std::move(snapshot.DiskStateChanges),
        snapshot.LastDiskStateSeqNo,
        std::move(snapshot.DirtyDevices),
        std::move(snapshot.DisksToCleanup),
        std::move(snapshot.ErrorNotifications),
        std::move(snapshot.UserNotifications),
        std::move(snapshot.OutdatedVolumeConfigs),
        std::move(snapshot.SuspendedDevices),
        std::move(snapshot.AutomaticallyReplacedDevices),
        std::move(snapshot.DiskRegistryAgentListParams));
}

void TDiskRegistryActor::CompleteLoadState(
    const TActorContext& ctx,
    TTxDiskRegistry::TLoadState& args)
{
    Y_ABORT_UNLESS(CurrentState == STATE_INIT);

    if (args.RestoreState) {
        BecomeAux(ctx, STATE_RESTORE);
    } else if (!args.Snapshot.WritableState) {
        BecomeAux(ctx, STATE_READ_ONLY);
    } else {
        BecomeAux(ctx, STATE_WORK);
    }

    // allow pipes to connect
    SignalTabletActive(ctx);

    // resend pending requests
    SendPendingRequests(ctx, PendingRequests);

    for (const auto& agent: args.Snapshot.Agents) {
        if (agent.GetState() != NProto::AGENT_STATE_UNAVAILABLE) {
            // this event will be scheduled using NonReplicatedAgentMaxTimeout
            ScheduleRejectAgent(ctx, agent.GetAgentId(), 0);
        }
    }

    InitializeState(std::move(args.Snapshot));

    SecureErase(ctx);

    ScheduleCleanup(ctx);

    DestroyBrokenDisks(ctx);

    ReallocateDisks(ctx);

    NotifyUsers(ctx);

    PublishDiskStates(ctx);

    UpdateCounters(ctx);

    StartMigration(ctx);

    UpdateVolumeConfigs(ctx);

    ProcessAutomaticallyReplacedDevices(ctx);

    ScheduleMakeBackup(ctx, args.LastBackupTime);

    ScheduleDiskRegistryAgentListExpiredParamsCleanup(ctx);
}

}   // namespace NCloud::NBlockStore::NStorage
