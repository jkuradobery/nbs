#include "disk_registry_state.h"

#include "disk_registry_schema.h"

#include <cloud/blockstore/libs/diagnostics/critical_events.h>
#include <cloud/blockstore/libs/kikimr/events.h>
#include <cloud/blockstore/libs/storage/core/config.h>
#include <cloud/blockstore/libs/storage/core/disk_validation.h>
#include <cloud/blockstore/libs/storage/disk_common/monitoring_utils.h>
#include <cloud/storage/core/libs/common/error.h>
#include <cloud/storage/core/libs/common/format.h>
#include <cloud/storage/core/libs/common/helpers.h>
#include <cloud/storage/core/libs/common/verify.h>
#include <cloud/storage/core/libs/diagnostics/logging.h>

#include <library/cpp/monlib/service/pages/templates.h>

#include <util/generic/algorithm.h>
#include <util/generic/iterator_range.h>
#include <util/generic/overloaded.h>
#include <util/generic/size_literals.h>
#include <util/string/builder.h>
#include <util/string/join.h>

#include <tuple>

namespace NCloud::NBlockStore::NStorage {

namespace {

////////////////////////////////////////////////////////////////////////////////

constexpr TDuration CMS_UPDATE_STATE_TO_ONLINE_TIMEOUT = TDuration::Minutes(5);

////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TTableCount;

template<typename ... Ts>
struct TTableCount<NKikimr::NIceDb::Schema::SchemaTables<Ts...>>
{
    enum { value = sizeof...(Ts) };
};

////////////////////////////////////////////////////////////////////////////////

struct TByUUID
{
    template <typename T, typename U>
    bool operator () (const T& lhs, const U& rhs) const
    {
        return lhs.GetDeviceUUID() < rhs.GetDeviceUUID();
    }
};

NProto::EDiskState ToDiskState(NProto::EAgentState agentState)
{
    switch (agentState)
    {
    case NProto::AGENT_STATE_ONLINE:
        return NProto::DISK_STATE_ONLINE;
    case NProto::AGENT_STATE_WARNING:
        return NProto::DISK_STATE_MIGRATION;
    case NProto::AGENT_STATE_UNAVAILABLE:
        return NProto::DISK_STATE_TEMPORARILY_UNAVAILABLE;
    default:
        Y_FAIL("unknown agent state");
    }
}

NProto::EDiskState ToDiskState(NProto::EDeviceState deviceState)
{
    switch (deviceState)
    {
    case NProto::DEVICE_STATE_ONLINE:
        return NProto::DISK_STATE_ONLINE;
    case NProto::DEVICE_STATE_WARNING:
        return NProto::DISK_STATE_MIGRATION;
    case NProto::DEVICE_STATE_ERROR:
        return NProto::DISK_STATE_ERROR;
    default:
        Y_FAIL("unknown device state");
    }
}

////////////////////////////////////////////////////////////////////////////////

TString GetMirroredDiskGroupId(const TString& diskId)
{
    return diskId + "/g";
}

TString GetReplicaDiskId(const TString& diskId, ui32 i)
{
    return TStringBuilder() << diskId << "/" << i;
}

////////////////////////////////////////////////////////////////////////////////

TDuration GetInfraTimeout(
    const TStorageConfig& config,
    NProto::EAgentState agentState)
{
    if (agentState == NProto::AGENT_STATE_UNAVAILABLE) {
        return config.GetNonReplicatedInfraUnavailableAgentTimeout();
    }

    return config.GetNonReplicatedInfraTimeout();
}

////////////////////////////////////////////////////////////////////////////////

THashMap<TString, NProto::TDevicePoolConfig> CreateDevicePoolConfigs(
    const NProto::TDiskRegistryConfig& config,
    const TStorageConfig& storageConfig)
{
    NProto::TDevicePoolConfig nonrepl;
    nonrepl.SetAllocationUnit(
        storageConfig.GetAllocationUnitNonReplicatedSSD() * 1_GB);

    THashMap<TString, NProto::TDevicePoolConfig> result {
        { TString {}, nonrepl }
    };

    for (const auto& pool: config.GetDevicePoolConfigs()) {
        result[pool.GetName()] = pool;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////

bool PlacementGroupMustHavePartitions(
    const NProto::TPlacementGroupConfig& placementGroup)
{
    switch (placementGroup.GetPlacementStrategy()) {
        case NProto::PLACEMENT_STRATEGY_SPREAD:
            return false;
        case NProto::PLACEMENT_STRATEGY_PARTITION:
            return true;
        default:
            return false;
    }
}

////////////////////////////////////////////////////////////////////////////////

bool PartitionSuitsPlacementGroup(
    const NProto::TPlacementGroupConfig& placementGroup,
    const ui32 placementPartitionIndex)
{
    if (!PlacementGroupMustHavePartitions(placementGroup)) {
        return placementPartitionIndex == 0;
    }

    return placementPartitionIndex > 0 &&
           placementPartitionIndex <=
               placementGroup.GetPlacementPartitionCount();
}

////////////////////////////////////////////////////////////////////////////////

ui32 GetMaxDisksInPlacementGroup(
    const TStorageConfig& config,
    const NProto::TPlacementGroupConfig& g)
{
    if (g.GetSettings().GetMaxDisksInGroup()) {
        return g.GetSettings().GetMaxDisksInGroup();
    }

    if (g.GetPlacementStrategy() == NProto::PLACEMENT_STRATEGY_PARTITION) {
        return config.GetMaxDisksInPartitionPlacementGroup();
    }

    return config.GetMaxDisksInPlacementGroup();
}

////////////////////////////////////////////////////////////////////////////////

TString MakeMirroredDiskDeviceReplacementMessage(
    TStringBuf diskId,
    TStringBuf reason)
{
    TStringBuilder message;
    message << "MirroredDiskId=" << diskId << ", ReplacementReason=" << reason;
    return std::move(message);
}

////////////////////////////////////////////////////////////////////////////////

struct TDevicePoolCounters
{
    ui64 FreeBytes = 0;
    ui64 TotalBytes = 0;
    ui64 AllocatedDevices = 0;
    ui64 DirtyDevices = 0;
    ui64 DevicesInOnlineState = 0;
    ui64 DevicesInWarningState = 0;
    ui64 DevicesInErrorState = 0;
};

void SetDevicePoolCounters(
    TDiskRegistrySelfCounters::TDevicePoolCounters& counters,
    const TDevicePoolCounters& values)
{
    counters.FreeBytes->Set(values.FreeBytes);
    counters.TotalBytes->Set(values.TotalBytes);
    counters.AllocatedDevices->Set(values.AllocatedDevices);
    counters.DirtyDevices->Set(values.DirtyDevices);
    counters.DevicesInOnlineState->Set(values.DevicesInOnlineState);
    counters.DevicesInWarningState->Set(values.DevicesInWarningState);
    counters.DevicesInErrorState->Set(values.DevicesInErrorState);
}

////////////////////////////////////////////////////////////////////////////////

TString GetPoolNameForCounters(
    const TString& poolName,
    const NProto::EDevicePoolKind poolKind)
{
    if (poolKind == NProto::DEVICE_POOL_KIND_LOCAL) {
        return "local";
    } else if (poolKind == NProto::DEVICE_POOL_KIND_DEFAULT) {
        return "default";
    }

    return poolName;
}

}   // namespace

////////////////////////////////////////////////////////////////////////////////

TDiskRegistryState::TDiskRegistryState(
        ILoggingServicePtr logging,
        TStorageConfigPtr storageConfig,
        NMonitoring::TDynamicCountersPtr counters,
        NProto::TDiskRegistryConfig config,
        TVector<NProto::TAgentConfig> agents,
        TVector<NProto::TDiskConfig> disks,
        TVector<NProto::TPlacementGroupConfig> placementGroups,
        TVector<TBrokenDiskInfo> brokenDisks,
        TVector<TString> disksToReallocate,
        TVector<TDiskStateUpdate> diskStateUpdates,
        ui64 diskStateSeqNo,
        TVector<TDirtyDevice> dirtyDevices,
        TVector<TString> disksToCleanup,
        TVector<TString> errorNotifications,
        TVector<NProto::TUserNotification> userNotifications,
        TVector<TString> outdatedVolumeConfigs,
        TVector<NProto::TSuspendedDevice> suspendedDevices,
        TDeque<TAutomaticallyReplacedDeviceInfo> automaticallyReplacedDevices,
        THashMap<TString, NProto::TDiskRegistryAgentParams> diskRegistryAgentListParams)
    : Log(logging->CreateLog("BLOCKSTORE_DISK_REGISTRY"))
    , StorageConfig(std::move(storageConfig))
    , Counters(counters)
    , AgentList({
        static_cast<double>(
            StorageConfig->GetNonReplicatedAgentTimeoutGrowthFactor()),
        StorageConfig->GetNonReplicatedAgentMinTimeout(),
        StorageConfig->GetNonReplicatedAgentMaxTimeout(),
        StorageConfig->GetNonReplicatedAgentDisconnectRecoveryInterval(),
        StorageConfig->GetSerialNumberValidationEnabled(),
    }, counters, std::move(agents), std::move(diskRegistryAgentListParams), Log)
    , DeviceList([&] {
        TVector<TDeviceId> uuids;
        uuids.reserve(dirtyDevices.size());
        for (auto& [uuid, diskId]: dirtyDevices) {
            uuids.push_back(uuid);
        }

        return uuids;
    }(), std::move(suspendedDevices))
    , BrokenDisks(std::move(brokenDisks))
    , AutomaticallyReplacedDevices(std::move(automaticallyReplacedDevices))
    , CurrentConfig(std::move(config))
    , NotificationSystem {
        StorageConfig,
        std::move(errorNotifications),
        std::move(userNotifications),
        std::move(disksToReallocate),
        std::move(diskStateUpdates),
        diskStateSeqNo,
        std::move(outdatedVolumeConfigs)
    }
{
    for (const auto& x: AutomaticallyReplacedDevices) {
        AutomaticallyReplacedDeviceIds.insert(x.DeviceId);
    }

    ProcessConfig(CurrentConfig);
    ProcessDisks(std::move(disks));
    ProcessPlacementGroups(std::move(placementGroups));
    ProcessAgents();
    ProcessDisksToCleanup(std::move(disksToCleanup));
    ProcessDirtyDevices(std::move(dirtyDevices));

    FillMigrations();

    if (Counters) {
        TVector<TString> poolNames;
        for (const auto& x: DevicePoolConfigs) {
            const auto& pool = x.second;
            poolNames.push_back(
                GetPoolNameForCounters(pool.GetName(), pool.GetKind()));
        }
        SelfCounters.Init(poolNames, Counters);
    }
}

void TDiskRegistryState::AllowNotifications(
    const TDiskId& diskId,
    const TDiskState& disk)
{
    // currently we don't want to notify users about mirrored disks since they are not
    // supposed to break

    if (disk.MasterDiskId) {
        return;
    }

    if (disk.MediaKind == NProto::STORAGE_MEDIA_SSD_NONREPLICATED ||
        disk.MediaKind == NProto::STORAGE_MEDIA_HDD_NONREPLICATED ||
        disk.MediaKind == NProto::STORAGE_MEDIA_SSD_LOCAL)
    {
        NotificationSystem.AllowNotifications(diskId);
    }
}

void TDiskRegistryState::ProcessDisks(TVector<NProto::TDiskConfig> configs)
{
    for (auto& config: configs) {
        const auto& diskId = config.GetDiskId();
        auto& disk = Disks[config.GetDiskId()];
        disk.LogicalBlockSize = config.GetBlockSize();
        disk.Devices.reserve(config.DeviceUUIDsSize());
        disk.State = config.GetState();
        disk.StateTs = TInstant::MicroSeconds(config.GetStateTs());
        disk.CloudId = config.GetCloudId();
        disk.FolderId = config.GetFolderId();
        disk.UserId = config.GetUserId();
        disk.ReplicaCount = config.GetReplicaCount();
        disk.MasterDiskId = config.GetMasterDiskId();
        disk.MediaKind = NProto::STORAGE_MEDIA_SSD_NONREPLICATED;

        if (config.GetStorageMediaKind() != NProto::STORAGE_MEDIA_DEFAULT) {
            disk.MediaKind = config.GetStorageMediaKind();
        } else if (disk.ReplicaCount == 1) {
            disk.MediaKind = NProto::STORAGE_MEDIA_SSD_MIRROR2;
        } else if (disk.ReplicaCount == 2) {
            disk.MediaKind = NProto::STORAGE_MEDIA_SSD_MIRROR3;
        } else {
            Y_VERIFY_DEBUG(
                disk.ReplicaCount == 0,
                "unexpected ReplicaCount: %d",
                disk.ReplicaCount
            );
        }

        AllowNotifications(diskId, disk);

        for (const auto& uuid: config.GetDeviceUUIDs()) {
            disk.Devices.push_back(uuid);

            DeviceList.MarkDeviceAllocated(diskId, uuid);
        }

        if (disk.MasterDiskId) {
            ReplicaTable.AddReplica(disk.MasterDiskId, disk.Devices);
        }

        for (const auto& id: config.GetDeviceReplacementUUIDs()) {
            disk.DeviceReplacementIds.push_back(id);
        }

        for (auto& m: config.GetMigrations()) {
            ++DeviceMigrationsInProgress;

            disk.MigrationTarget2Source.emplace(
                m.GetTargetDevice().GetDeviceUUID(),
                m.GetSourceDeviceId());

            disk.MigrationSource2Target.emplace(
                m.GetSourceDeviceId(),
                m.GetTargetDevice().GetDeviceUUID());

            DeviceList.MarkDeviceAllocated(
                diskId,
                m.GetTargetDevice().GetDeviceUUID());
        }

        if (!config.GetFinishedMigrations().empty()) {
            ui64 seqNo = NotificationSystem.GetDiskSeqNo(diskId);
            if (!seqNo) {
                ReportDiskRegistryNoScheduledNotification(TStringBuilder()
                    << "No scheduled notification for disk " << diskId.Quote());

                seqNo = NotificationSystem.AddReallocateRequest(disk.MasterDiskId
                    ? disk.MasterDiskId
                    : diskId);
            }

            for (auto& m: config.GetFinishedMigrations()) {
                const auto& uuid = m.GetDeviceId();

                DeviceList.MarkDeviceAllocated(diskId, uuid);

                disk.FinishedMigrations.push_back({
                    .DeviceId = uuid,
                    .SeqNo = seqNo
                });
            }
        }
    }

    for (const auto& x: Disks) {
        if (x.second.ReplicaCount) {
            for (const auto& id: x.second.DeviceReplacementIds) {
                ReplicaTable.MarkReplacementDevice(x.first, id, true);
            }
        }
    }
}

void TDiskRegistryState::AddMigration(
    const TDiskState& disk,
    const TString& diskId,
    const TString& sourceDeviceId)
{
    if (disk.MediaKind == NProto::STORAGE_MEDIA_SSD_LOCAL) {
        return;
    }

    if (disk.MasterDiskId) {
        // mirrored disk replica
        if (!StorageConfig->GetMirroredMigrationStartAllowed()) {
            return;
        }
    } else {
        // nonreplicated disk
        if (!StorageConfig->GetNonReplicatedMigrationStartAllowed()) {
            return;
        }
    }

    Migrations.emplace(diskId, sourceDeviceId);
}

void TDiskRegistryState::FillMigrations()
{
    for (auto& [diskId, disk]: Disks) {
        if (disk.State == NProto::DISK_STATE_ONLINE) {
            continue;
        }

        for (const auto& uuid: disk.Devices) {
            if (disk.MigrationSource2Target.contains(uuid)) {
                continue; // migration in progress
            }

            auto [agent, device] = FindDeviceLocation(uuid);

            if (!device) {
                ReportDiskRegistryDeviceNotFound(
                    TStringBuilder() << "FillMigrations:DeviceId: " << uuid);

                continue;
            }

            if (!agent) {
                ReportDiskRegistryAgentNotFound(
                    TStringBuilder() << "FillMigrations:AgentId: "
                    << device->GetAgentId());

                continue;
            }

            if (device->GetState() == NProto::DEVICE_STATE_WARNING) {
                AddMigration(disk, diskId, uuid);

                continue;
            }

            if (device->GetState() == NProto::DEVICE_STATE_ERROR) {
                continue; // skip for broken devices
            }

            if (agent->GetState() == NProto::AGENT_STATE_WARNING) {
                AddMigration(disk, diskId, uuid);
            }
        }
    }
}

void TDiskRegistryState::ProcessPlacementGroups(
    TVector<NProto::TPlacementGroupConfig> configs)
{
    for (auto& config: configs) {
        auto groupId = config.GetGroupId();

        for (const auto& disk: config.GetDisks()) {
            auto* d = Disks.FindPtr(disk.GetDiskId());

            if (!d) {
                ReportDiskRegistryDiskNotFound(TStringBuilder()
                    << "ProcessPlacementGroups:DiskId: " << disk.GetDiskId());

                continue;
            }

            d->PlacementGroupId = groupId;

            if (PlacementGroupMustHavePartitions(config)) {
                if (!PartitionSuitsPlacementGroup(
                        config,
                        disk.GetPlacementPartitionIndex()))
                {
                    ReportDiskRegistryInvalidPlacementGroupPartition(
                        TStringBuilder()
                        << "ProcessPlacementGroups:DiskId: " << disk.GetDiskId()
                        << ", PlacementGroupId: " << config.GetGroupId()
                        << ", Strategy: "
                        << NProto::EPlacementStrategy_Name(
                               config.GetPlacementStrategy())
                        << ", PlacementPartitionIndex: "
                        << disk.GetPlacementPartitionIndex());
                    continue;
                }
                d->PlacementPartitionIndex = disk.GetPlacementPartitionIndex();
            }
        }

        PlacementGroups[groupId] = std::move(config);
    }
}

void TDiskRegistryState::ProcessAgents()
{
    for (auto& agent: AgentList.GetAgents()) {
        DeviceList.UpdateDevices(agent);
        TimeBetweenFailures.SetWorkTime(
            TimeBetweenFailures.GetWorkTime() +
            agent.GetTimeBetweenFailures().GetWorkTime());
        TimeBetweenFailures.SetBrokenCount(
            TimeBetweenFailures.GetBrokenCount() +
            agent.GetTimeBetweenFailures().GetBrokenCount());
    }
}

void TDiskRegistryState::ProcessDisksToCleanup(TVector<TString> disksToCleanup)
{
    for (auto& diskId: disksToCleanup) {
        DisksToCleanup.emplace(std::move(diskId));
    }
}

void TDiskRegistryState::ProcessDirtyDevices(TVector<TDirtyDevice> dirtyDevices)
{
    for (auto&& [uuid, diskId]: dirtyDevices) {
        PendingCleanup.Insert(std::move(diskId), std::move(uuid));
    }
}

const TVector<NProto::TAgentConfig>& TDiskRegistryState::GetAgents() const
{
    return AgentList.GetAgents();
}

NProto::TError TDiskRegistryState::ValidateAgent(
    const NProto::TAgentConfig& agent) const
{
    const auto& agentId = agent.GetAgentId();

    if (agentId.empty()) {
        return MakeError(E_ARGUMENT, "empty agent id");
    }

    if (agent.GetNodeId() == 0) {
        return MakeError(E_ARGUMENT, "empty node id");
    }

    auto* buddy = AgentList.FindAgent(agent.GetNodeId());

    if (buddy
        && buddy->GetAgentId() != agentId
        && buddy->GetState() != NProto::AGENT_STATE_UNAVAILABLE)
    {
        return MakeError(E_INVALID_STATE, TStringBuilder()
            << "Agent " << buddy->GetAgentId().Quote()
            << " already registered at node #" << agent.GetNodeId());
    }

    buddy = AgentList.FindAgent(agentId);

    if (buddy
        && buddy->GetSeqNumber() > agent.GetSeqNumber()
        && buddy->GetState() != NProto::AGENT_STATE_UNAVAILABLE)
    {
        return MakeError(E_INVALID_STATE, TStringBuilder()
            << "Agent " << buddy->GetAgentId().Quote()
            << " already registered with a greater SeqNo "
            << "(" << buddy->GetSeqNumber() << " > " << agent.GetSeqNumber() << ")");
    }

    const TKnownAgent* knownAgent = KnownAgents.FindPtr(agentId);

    if (!knownAgent) {
        return {};
    }

    TString rack;
    if (agent.DevicesSize()) {
        rack = agent.GetDevices(0).GetRack();
    }

    for (const auto& device: agent.GetDevices()) {
        // right now we suppose that each agent presents devices
        // from one rack only
        if (rack != device.GetRack()) {
            return MakeError(E_ARGUMENT, TStringBuilder()
                << "all agent devices should come from the same rack, mismatch: "
                << rack << " != " << device.GetRack());
        }
    }

    return {};
}

auto TDiskRegistryState::FindDisk(const TDeviceId& uuid) const -> TDiskId
{
    return DeviceList.FindDiskId(uuid);
}

void TDiskRegistryState::AdjustDeviceIfNeeded(
    NProto::TDeviceConfig& device,
    TInstant timestamp)
{
    if (!device.GetUnadjustedBlockCount()) {
        device.SetUnadjustedBlockCount(device.GetBlocksCount());
    }

    const auto* poolConfig = DevicePoolConfigs.FindPtr(device.GetPoolName());
    if (!poolConfig) {
        device.SetState(NProto::DEVICE_STATE_ERROR);
        device.SetStateTs(timestamp.MicroSeconds());
        device.SetStateMessage(
            Sprintf("unknown pool: %s", device.GetPoolName().c_str()));

        return;
    }

    device.SetPoolKind(poolConfig->GetKind());

    const ui64 unit = poolConfig->GetAllocationUnit();
    Y_VERIFY_DEBUG(unit != 0);

    if (device.GetState() == NProto::DEVICE_STATE_ERROR ||
        !DeviceList.FindDiskId(device.GetDeviceUUID()).empty())
    {
        return;
    }

    const auto deviceSize = device.GetBlockSize() * device.GetBlocksCount();

    if (deviceSize < unit) {
        device.SetState(NProto::DEVICE_STATE_ERROR);
        device.SetStateTs(timestamp.MicroSeconds());
        device.SetStateMessage(TStringBuilder()
            << "device is too small: " << deviceSize);

        return;
    }

    if (device.GetBlockSize() == 0 || unit % device.GetBlockSize() != 0) {
        device.SetState(NProto::DEVICE_STATE_ERROR);
        device.SetStateTs(timestamp.MicroSeconds());
        device.SetStateMessage(TStringBuilder()
            << "bad block size: " << device.GetBlockSize());

        return;
    }

    if (deviceSize > unit) {
        device.SetBlocksCount(unit / device.GetBlockSize());
    }
}

void TDiskRegistryState::RemoveAgentFromNode(
    TDiskRegistryDatabase& db,
    NProto::TAgentConfig& agent,
    TInstant timestamp,
    TVector<TDiskId>* affectedDisks,
    TVector<TDiskId>* disksToReallocate)
{
    Y_VERIFY_DEBUG(agent.GetState() == NProto::AGENT_STATE_UNAVAILABLE);

    const ui32 nodeId = agent.GetNodeId();
    agent.SetNodeId(0);
    ChangeAgentState(
        agent,
        NProto::AGENT_STATE_UNAVAILABLE,
        timestamp,
        "lost");

    THashSet<TDiskId> diskIds;

    for (auto& d: *agent.MutableDevices()) {
        d.SetNodeId(0);

        const auto& uuid = d.GetDeviceUUID();
        auto diskId = DeviceList.FindDiskId(uuid);

        if (!diskId.empty()) {
            diskIds.emplace(std::move(diskId));
        }
    }

    AgentList.RemoveAgentFromNode(nodeId);
    DeviceList.UpdateDevices(agent, nodeId);

    for (const auto& id: diskIds) {
        AddReallocateRequest(db, id);
        disksToReallocate->push_back(id);
    }

    db.UpdateAgent(agent);
    db.DeleteOldAgent(nodeId);

    ApplyAgentStateChange(db, agent, timestamp, *affectedDisks);
}

NProto::TError TDiskRegistryState::RegisterAgent(
    TDiskRegistryDatabase& db,
    NProto::TAgentConfig config,
    TInstant timestamp,
    TVector<TDiskId>* affectedDisks,
    TVector<TDiskId>* disksToReallocate)
{
    if (auto error = ValidateAgent(config); HasError(error)) {
        return error;
    }

    try {
        if (auto* buddy = AgentList.FindAgent(config.GetNodeId());
                buddy && buddy->GetAgentId() != config.GetAgentId())
        {
            STORAGE_INFO(
                "Agent %s occupies the same node (#%d) as the arriving agent %s. "
                "Kick out %s",
                buddy->GetAgentId().c_str(),
                config.GetNodeId(),
                config.GetAgentId().c_str(),
                buddy->GetAgentId().c_str());

            RemoveAgentFromNode(
                db,
                *buddy,
                timestamp,
                affectedDisks,
                disksToReallocate);
        }

        const auto& knownAgent = KnownAgents.Value(
            config.GetAgentId(),
            TKnownAgent {});

        const auto prevNodeId = AgentList.FindNodeId(config.GetAgentId());

        THashSet<TDeviceId> newDevices;

        auto& agent = AgentList.RegisterAgent(
            std::move(config),
            timestamp,
            knownAgent,
            &newDevices);

        for (auto& d: *agent.MutableDevices()) {
            AdjustDeviceIfNeeded(d, timestamp);

            const auto& uuid = d.GetDeviceUUID();

            if (!StorageConfig->GetNonReplicatedDontSuspendDevices()
                    && d.GetPoolKind() == NProto::DEVICE_POOL_KIND_LOCAL
                    && newDevices.contains(uuid))
            {
                SuspendDevice(db, uuid);
            }
        }

        DeviceList.UpdateDevices(agent, prevNodeId);

        for (const auto& uuid: newDevices) {
            if (!DeviceList.FindDiskId(uuid)) {
                DeviceList.MarkDeviceAsDirty(uuid);
                db.UpdateDirtyDevice(uuid, {});
            }
        }

        THashSet<TDiskId> diskIds;

        for (const auto& d: agent.GetDevices()) {
            const auto& uuid = d.GetDeviceUUID();
            auto diskId = DeviceList.FindDiskId(uuid);

            if (diskId.empty()) {
                continue;
            }

            if (d.GetState() == NProto::DEVICE_STATE_ERROR) {
                auto& disk = Disks[diskId];
                if (!RestartDeviceMigration(db, diskId, disk, uuid)) {
                    CancelDeviceMigration(db, diskId, disk, uuid);
                }
            }

            diskIds.emplace(std::move(diskId));
        }

        for (auto& id: diskIds) {
            if (TryUpdateDiskState(db, id, timestamp)) {
                affectedDisks->push_back(id);
            }
        }

        if (prevNodeId != agent.GetNodeId()) {
            db.DeleteOldAgent(prevNodeId);

            for (const auto& id: diskIds) {
                AddReallocateRequest(db, id);
                disksToReallocate->push_back(id);
            }
        }

        if (agent.GetState() == NProto::AGENT_STATE_UNAVAILABLE) {
            agent.SetCmsTs(0);
            ChangeAgentState(
                agent,
                NProto::AGENT_STATE_WARNING,
                timestamp,
                "back from unavailable");

            ApplyAgentStateChange(db, agent, timestamp, *affectedDisks);
        }

        UpdateAgent(db, agent);

    } catch (const TServiceError& e) {
        return MakeError(e.GetCode(), e.what());
    } catch (...) {
        return MakeError(E_FAIL, CurrentExceptionMessage());
    }

    return {};
}

NProto::TError TDiskRegistryState::UnregisterAgent(
    TDiskRegistryDatabase& db,
    ui32 nodeId)
{
    if (!RemoveAgent(db, nodeId)) {
        return MakeError(S_ALREADY, "agent not found");
    }

    return {};
}

void TDiskRegistryState::RebuildDiskPlacementInfo(
    const TDiskState& disk,
    NProto::TPlacementGroupConfig::TDiskInfo* diskInfo) const
{
    Y_VERIFY(diskInfo);

    THashSet<TString> racks;

    for (const auto& uuid: disk.Devices) {
        auto rack = DeviceList.FindRack(uuid);
        if (rack) {
            racks.insert(std::move(rack));
        }
    }

    for (auto& [targetId, sourceId]: disk.MigrationTarget2Source) {
        auto rack = DeviceList.FindRack(targetId);
        if (rack) {
            racks.insert(std::move(rack));
        }
    }

    diskInfo->MutableDeviceRacks()->Assign(
        std::make_move_iterator(racks.begin()),
        std::make_move_iterator(racks.end())
    );

    diskInfo->SetPlacementPartitionIndex(disk.PlacementPartitionIndex);
}

TResultOrError<NProto::TDeviceConfig> TDiskRegistryState::AllocateReplacementDevice(
    TDiskRegistryDatabase& db,
    const TString& diskId,
    const TDeviceId& deviceReplacementId,
    const TDeviceList::TAllocationQuery& query,
    TInstant timestamp,
    TString message)
{
    if (deviceReplacementId.empty()) {
        auto device = DeviceList.AllocateDevice(diskId, query);
        if (device.GetDeviceUUID().empty()) {
            return MakeError(E_BS_DISK_ALLOCATION_FAILED, "can't allocate device");
        }

        return device;
    }

    auto [device, error] = DeviceList.AllocateSpecificDevice(
        diskId,
        deviceReplacementId,
        query);
    if (HasError(error)) {
        return MakeError(E_BS_DISK_ALLOCATION_FAILED, TStringBuilder()
            << "can't allocate specific device "
            << deviceReplacementId.Quote()
            << " : " << error.GetMessage());
    }

    // replacement device can come from dirty devices list
    db.DeleteDirtyDevice(deviceReplacementId);

    // replacement device can come from automatically replaced devices list
    if (IsAutomaticallyReplaced(deviceReplacementId)) {
        DeleteAutomaticallyReplacedDevice(db, deviceReplacementId);
    }

    AdjustDeviceState(
        db,
        device,
        NProto::DEVICE_STATE_ONLINE,
        timestamp,
        std::move(message));

    return device;
}

NProto::TError TDiskRegistryState::ReplaceDevice(
    TDiskRegistryDatabase& db,
    const TString& diskId,
    const TString& deviceId,
    const TString& deviceReplacementId,
    TInstant timestamp,
    TString message,
    bool manual,
    bool* diskStateUpdated)
{
    try {
        if (!diskId) {
            return MakeError(E_ARGUMENT, "empty disk id");
        }

        if (!deviceId) {
            return MakeError(E_ARGUMENT, "empty device id");
        }

        if (DeviceList.FindDiskId(deviceId) != diskId) {
            return MakeError(E_ARGUMENT, TStringBuilder()
                << "device does not belong to disk " << diskId.Quote());
        }

        if (!Disks.contains(diskId)) {
            return MakeError(E_ARGUMENT, TStringBuilder()
                << "unknown disk: " << diskId.Quote());
        }

        TDiskState& disk = Disks[diskId];

        auto it = Find(disk.Devices, deviceId);
        if (it == disk.Devices.end()) {
            auto message = ReportDiskRegistryDeviceNotFound(
                TStringBuilder() << "ReplaceDevice:DiskId: " << diskId
                << ", DeviceId: " << deviceId);

            return MakeError(E_FAIL, message);
        }

        auto [agentPtr, devicePtr] = FindDeviceLocation(deviceId);
        if (!agentPtr || !devicePtr) {
            return MakeError(E_INVALID_STATE, "can't find device");
        }

        const ui64 logicalBlockCount = devicePtr->GetBlockSize() * devicePtr->GetBlocksCount()
            / disk.LogicalBlockSize;

        TDeviceList::TAllocationQuery query {
            .ForbiddenRacks = CollectForbiddenRacks(diskId, disk, "ReplaceDevice"),
            .PreferredRacks = CollectPreferredRacks(diskId),
            .LogicalBlockSize = disk.LogicalBlockSize,
            .BlockCount = logicalBlockCount,
            .PoolName = devicePtr->GetPoolName(),
            .PoolKind = GetDevicePoolKind(devicePtr->GetPoolName())
        };

        if (query.PoolKind == NProto::DEVICE_POOL_KIND_LOCAL) {
            query.NodeIds = { devicePtr->GetNodeId() };
        }


        auto [targetDevice, error] = AllocateReplacementDevice(
            db,
            diskId,
            deviceReplacementId,
            query,
            timestamp,
            message);
        if (HasError(error)) {
            TryUpdateDiskState(db, diskId, timestamp);
            return error;
        }

        AdjustDeviceBlockCount(
            timestamp,
            db,
            targetDevice,
            logicalBlockCount * disk.LogicalBlockSize / targetDevice.GetBlockSize()
        );

        if (disk.MasterDiskId) {
            auto* masterDisk = Disks.FindPtr(disk.MasterDiskId);
            Y_VERIFY_DEBUG(masterDisk);
            if (masterDisk) {
                auto it = Find(
                    masterDisk->DeviceReplacementIds.begin(),
                    masterDisk->DeviceReplacementIds.end(),
                    deviceId);

                if (it != masterDisk->DeviceReplacementIds.end()) {
                    // source device was a replacement device already, let's
                    // just change it inplace
                    *it = targetDevice.GetDeviceUUID();
                } else {
                    masterDisk->DeviceReplacementIds.push_back(
                        targetDevice.GetDeviceUUID());
                }

                db.UpdateDisk(BuildDiskConfig(disk.MasterDiskId, *masterDisk));

                const bool replaced = ReplicaTable.ReplaceDevice(
                    disk.MasterDiskId,
                    deviceId,
                    targetDevice.GetDeviceUUID());

                Y_VERIFY_DEBUG(replaced);
            }
        }

        if (manual) {
            devicePtr->SetState(NProto::DEVICE_STATE_ERROR);
        } else {
            TAutomaticallyReplacedDeviceInfo deviceInfo{deviceId, timestamp};
            AutomaticallyReplacedDevices.push_back(deviceInfo);
            AutomaticallyReplacedDeviceIds.insert(deviceId);
            db.AddAutomaticallyReplacedDevice(deviceInfo);
        }
        devicePtr->SetStateMessage(std::move(message));
        devicePtr->SetStateTs(timestamp.MicroSeconds());

        DeviceList.UpdateDevices(*agentPtr);

        DeviceList.ReleaseDevice(deviceId);
        db.UpdateDirtyDevice(deviceId, diskId);

        CancelDeviceMigration(db, diskId, disk, deviceId);

        *it = targetDevice.GetDeviceUUID();

        *diskStateUpdated = TryUpdateDiskState(db, diskId, disk, timestamp);

        UpdateAgent(db, *agentPtr);

        UpdatePlacementGroup(db, diskId, disk, "ReplaceDevice");
        UpdateAndReallocateDisk(db, diskId, disk);

        PendingCleanup.Insert(diskId, deviceId);
    } catch (const TServiceError& e) {
        return MakeError(e.GetCode(), e.what());
    }

    return {};
}

void TDiskRegistryState::AdjustDeviceBlockCount(
    TInstant now,
    TDiskRegistryDatabase& db,
    NProto::TDeviceConfig& device,
    ui64 newBlockCount)
{
    Y_UNUSED(now);
    if (newBlockCount > device.GetUnadjustedBlockCount()) {
        ReportDiskRegistryBadDeviceSizeAdjustment(
            TStringBuilder() << "AdjustDeviceBlockCount:DeviceId: "
            << device.GetDeviceUUID()
            << ", UnadjustedBlockCount: " << device.GetUnadjustedBlockCount()
            << ", newBlockCount: " << newBlockCount);

        return;
    }

    if (newBlockCount == device.GetBlocksCount()) {
        return;
    }

    auto [agent, source] = FindDeviceLocation(device.GetDeviceUUID());
    if (!agent || !source) {
        ReportDiskRegistryBadDeviceSizeAdjustment(
            TStringBuilder() << "AdjustDeviceBlockCount:DeviceId: "
            << device.GetDeviceUUID()
            << ", agent?: " << !!agent
            << ", source?: " << !!source);

        return;
    }

    source->SetBlocksCount(newBlockCount);

    UpdateAgent(db, *agent);
    DeviceList.UpdateDevices(*agent);

    device = *source;
}

void TDiskRegistryState::AdjustDeviceState(
    TDiskRegistryDatabase& db,
    NProto::TDeviceConfig& device,
    NProto::EDeviceState state,
    TInstant timestamp,
    TString message)
{
    if (device.GetState() == state) {
        return;
    }

    auto [agent, source] = FindDeviceLocation(device.GetDeviceUUID());
    if (!agent || !source) {
        ReportDiskRegistryBadDeviceStateAdjustment(
            TStringBuilder() << "AdjustDeviceState:DeviceId: "
            << device.GetDeviceUUID()
            << ", agent?: " << !!agent
            << ", source?: " << !!source);
        return;
    }

    source->SetState(state);
    source->SetStateTs(timestamp.MicroSeconds());
    source->SetStateMessage(std::move(message));

    UpdateAgent(db, *agent);
    DeviceList.UpdateDevices(*agent);

    device = *source;
}

ui64 TDiskRegistryState::GetDeviceBlockCountWithOverrides(
    const TDiskId& diskId,
    const NProto::TDeviceConfig& device)
{
    auto deviceBlockCount = device.GetBlocksCount();

    if (const auto* overrides = DeviceOverrides.FindPtr(diskId)) {
        const auto* overriddenBlockCount =
            overrides->Device2BlockCount.FindPtr(device.GetDeviceUUID());
        if (overriddenBlockCount) {
            deviceBlockCount = *overriddenBlockCount;
        }
    }

    return deviceBlockCount;
}

bool TDiskRegistryState::UpdatePlacementGroup(
    TDiskRegistryDatabase& db,
    const TDiskId& diskId,
    const TDiskState& disk,
    TStringBuf callerName)
{
    if (disk.PlacementGroupId.empty()) {
        return false;
    }

    auto* pg = PlacementGroups.FindPtr(disk.PlacementGroupId);
    if (!pg) {
        ReportDiskRegistryPlacementGroupNotFound(
            TStringBuilder() << callerName << ":DiskId: " << diskId
                << ", PlacementGroupId: " << disk.PlacementGroupId);
        return false;
    }

    auto* diskInfo = FindIfPtr(*pg->Config.MutableDisks(), [&] (auto& disk) {
        return disk.GetDiskId() == diskId;
    });

    if (!diskInfo) {
        ReportDiskRegistryPlacementGroupDiskNotFound(
            TStringBuilder() << callerName << ":DiskId: " << diskId
                << ", PlacementGroupId: " << disk.PlacementGroupId);

        return false;
    }

    RebuildDiskPlacementInfo(disk, diskInfo);

    pg->Config.SetConfigVersion(pg->Config.GetConfigVersion() + 1);
    db.UpdatePlacementGroup(pg->Config);

    return true;
}

TDeviceList::TAllocationQuery TDiskRegistryState::MakeMigrationQuery(
    const TDiskId& sourceDiskId,
    const NProto::TDeviceConfig& sourceDevice)
{
    TDiskState& disk = Disks[sourceDiskId];

    const auto logicalBlockCount =
        GetDeviceBlockCountWithOverrides(sourceDiskId, sourceDevice)
        * sourceDevice.GetBlockSize()
        / disk.LogicalBlockSize;

    TDeviceList::TAllocationQuery query {
        .ForbiddenRacks = CollectForbiddenRacks(sourceDiskId, disk, "StartDeviceMigration"),
        .PreferredRacks = CollectPreferredRacks(sourceDiskId),
        .LogicalBlockSize = disk.LogicalBlockSize,
        .BlockCount = logicalBlockCount,
        .PoolName = sourceDevice.GetPoolName(),
        .PoolKind = GetDevicePoolKind(sourceDevice.GetPoolName())
    };

    if (query.PoolKind == NProto::DEVICE_POOL_KIND_LOCAL) {
        query.NodeIds = { sourceDevice.GetNodeId() };
    }

    return query;
}

NProto::TError TDiskRegistryState::ValidateStartDeviceMigration(
    const TDiskId& sourceDiskId,
    const TString& sourceDeviceId)
{
    if (!Disks.contains(sourceDiskId)) {
        return MakeError(E_NOT_FOUND, TStringBuilder() <<
            "disk " << sourceDiskId.Quote() << " not found");
    }

    if (DeviceList.FindDiskId(sourceDeviceId) != sourceDiskId) {
        ReportDiskRegistryDeviceDoesNotBelongToDisk(
            TStringBuilder() << "StartDeviceMigration:DiskId: "
            << sourceDiskId << ", DeviceId: " << sourceDeviceId);

        return MakeError(E_ARGUMENT, TStringBuilder() <<
            "device " << sourceDeviceId.Quote() << " does not belong to "
                << sourceDiskId.Quote());
    }

    if (!DeviceList.FindDevice(sourceDeviceId)) {
        return MakeError(E_NOT_FOUND, TStringBuilder() <<
            "device " << sourceDeviceId.Quote() << " not found");
    }

    return {};
}

NProto::TDeviceConfig TDiskRegistryState::StartDeviceMigrationImpl(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TDiskId& sourceDiskId,
    const TDeviceId& sourceDeviceId,
    NProto::TDeviceConfig targetDevice)
{
    TDiskState& disk = Disks[sourceDiskId];

    const NProto::TDeviceConfig* sourceDevice =
        DeviceList.FindDevice(sourceDeviceId);

    const auto logicalBlockCount =
        GetDeviceBlockCountWithOverrides(sourceDiskId, *sourceDevice)
        * sourceDevice->GetBlockSize() / disk.LogicalBlockSize;

    AdjustDeviceBlockCount(
        now,
        db,
        targetDevice,
        logicalBlockCount * disk.LogicalBlockSize / targetDevice.GetBlockSize()
    );

    disk.MigrationTarget2Source[targetDevice.GetDeviceUUID()]
        = sourceDeviceId;
    disk.MigrationSource2Target[sourceDeviceId]
        = targetDevice.GetDeviceUUID();
    ++DeviceMigrationsInProgress;

    DeleteDeviceMigration(sourceDiskId, sourceDeviceId);

    UpdatePlacementGroup(db, sourceDiskId, disk, "StartDeviceMigration");
    UpdateAndReallocateDisk(db, sourceDiskId, disk);

    DeviceList.MarkDeviceAllocated(sourceDiskId, targetDevice.GetDeviceUUID());

    return targetDevice;
}

/*
 *             | online | warn | unavailable |
 * online      |   -    |  1   |     2       |
 * warn        |   3    |  -   |     4       |
 * unavailable |   5    |  6   |     -       |
 * 1 - cms request - infra wants to exclude a host from our cluster for maintenance
 * 2 - agent is unavailable for some time
 * 3 - cms request - infra returns the host; blockstore-client request
 * 4 - agent is unavailable for some time
 * 5 - ERROR
 * 6 - agent is available again
 */

void TDiskRegistryState::ChangeAgentState(
    NProto::TAgentConfig& agent,
    NProto::EAgentState newState,
    TInstant now,
    TString stateMessage)
{
    const bool isChangedSate = newState != agent.GetState();
    if (isChangedSate) {
        if (newState == NProto::AGENT_STATE_UNAVAILABLE) {
            auto increment = [] (auto& mtbf, auto& workTime) {
                mtbf.SetWorkTime(mtbf.GetWorkTime() + workTime.Seconds());
                mtbf.SetBrokenCount(mtbf.GetBrokenCount() + 1);
            };

            if (agent.GetState() != NProto::AGENT_STATE_WARNING ||
                HasDependentDisks(agent))
            {
                const TDuration workTime = agent.GetWorkTs()
                    ? (now - TInstant::Seconds(agent.GetWorkTs()))
                    : (now - TInstant::MicroSeconds(agent.GetStateTs()));

                increment(*agent.MutableTimeBetweenFailures(), workTime);
                increment(TimeBetweenFailures, workTime);
            }
            agent.SetWorkTs({});
        } else if (agent.GetState() == NProto::AGENT_STATE_UNAVAILABLE) {
            agent.SetWorkTs(now.Seconds());
        }
    }

    agent.SetState(newState);
    agent.SetStateTs(now.MicroSeconds());
    agent.SetStateMessage(std::move(stateMessage));
}

TResultOrError<NProto::TDeviceConfig> TDiskRegistryState::StartDeviceMigration(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TDiskId& sourceDiskId,
    const TDeviceId& sourceDeviceId)
{
    try {
        if (auto error = ValidateStartDeviceMigration(
            sourceDiskId,
            sourceDeviceId); HasError(error))
        {
            return error;
        }

        TDeviceList::TAllocationQuery query =
            MakeMigrationQuery(
                sourceDiskId,
                *DeviceList.FindDevice(sourceDeviceId));

        NProto::TDeviceConfig targetDevice
            = DeviceList.AllocateDevice(sourceDiskId, query);
        if (targetDevice.GetDeviceUUID().empty()) {
            return MakeError(E_BS_DISK_ALLOCATION_FAILED, TStringBuilder() <<
                "can't allocate target for " << sourceDeviceId.Quote());
        }

        return StartDeviceMigrationImpl(
            now, db, sourceDiskId, sourceDeviceId, std::move(targetDevice));
    } catch (const TServiceError& e) {
        return MakeError(e.GetCode(), e.what());
    }
}


TResultOrError<NProto::TDeviceConfig> TDiskRegistryState::StartDeviceMigration(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TDiskId& sourceDiskId,
    const TDeviceId& sourceDeviceId,
    const TDeviceId& targetDeviceId)
{
    try {
        if (auto error = ValidateStartDeviceMigration(
            sourceDiskId,
            sourceDeviceId); HasError(error))
        {
            return error;
        }

        TDeviceList::TAllocationQuery query =
            MakeMigrationQuery(
                sourceDiskId,
                *DeviceList.FindDevice(sourceDeviceId));

        const NProto::TDeviceConfig* targetDevice =
            DeviceList.FindDevice(targetDeviceId);
        if (!targetDevice) {
            return MakeError(E_NOT_FOUND, TStringBuilder() <<
                "can't find target device " << targetDeviceId.Quote());
        }

        if (!DeviceList.ValidateAllocationQuery(query, targetDeviceId)) {
            return MakeError(E_BS_DISK_ALLOCATION_FAILED, TStringBuilder()
                << "can't migrate from " << sourceDeviceId.Quote()
                << " to " << targetDeviceId.Quote());
        }

        DeviceList.MarkDeviceAllocated(sourceDiskId, targetDeviceId);
        return StartDeviceMigrationImpl(
            now, db, sourceDiskId, sourceDeviceId, *targetDevice);
    } catch (const TServiceError& e) {
        return MakeError(e.GetCode(), e.what());
    }
}

ui32 TDiskRegistryState::CalculateRackCount() const
{
    THashSet<TStringBuf> racks;

    for (const auto& agent: AgentList.GetAgents()) {
        for (const auto& device: agent.GetDevices()) {
            racks.insert(device.GetRack());
        }
    }

    return racks.size();
}

TDeque<TRackInfo> TDiskRegistryState::GatherRacksInfo(
    const TString& poolName) const
{
    TDeque<TRackInfo> racks;
    THashMap<TString, TRackInfo*> m;

    for (const auto& agent: AgentList.GetAgents()) {
        TRackInfo::TAgentInfo* agentInfo = nullptr;

        for (const auto& device: agent.GetDevices()) {
            if (device.GetPoolName() != poolName) {
                continue;
            }

            auto& rackPtr = m[device.GetRack()];
            if (!rackPtr) {
                racks.emplace_back(device.GetRack());
                rackPtr = &racks.back();
            }

            if (!agentInfo) {
                agentInfo = FindIfPtr(
                    rackPtr->AgentInfos,
                    [&] (const TRackInfo::TAgentInfo& info) {
                        return info.AgentId == agent.GetAgentId();
                    });
            }

            if (!agentInfo) {
                agentInfo = &rackPtr->AgentInfos.emplace_back(
                    agent.GetAgentId(),
                    agent.GetNodeId()
                );
            }

            ++agentInfo->TotalDevices;

            if (device.GetState() == NProto::DEVICE_STATE_ERROR) {
                ++agentInfo->BrokenDevices;
            }

            if (auto diskId = DeviceList.FindDiskId(device.GetDeviceUUID())) {
                ++agentInfo->AllocatedDevices;

                auto* disk = Disks.FindPtr(diskId);
                if (disk) {
                    if (disk->PlacementGroupId) {
                        auto& placementPartitions = rackPtr->PlacementGroups[disk->PlacementGroupId];
                        if (disk->PlacementPartitionIndex) {
                            placementPartitions.insert(disk->PlacementPartitionIndex);
                        }
                    }
                } else {
                    ReportDiskRegistryDeviceListReferencesNonexistentDisk(
                        TStringBuilder() << "GatherRacksInfo:DiskId: " << diskId
                        << ", DeviceId: " << device.GetDeviceUUID());
                }
            } else if (device.GetState() == NProto::DEVICE_STATE_ONLINE) {
                switch (agent.GetState()) {
                    case NProto::AGENT_STATE_ONLINE: {
                        if (DeviceList.IsDirtyDevice(device.GetDeviceUUID())) {
                            ++agentInfo->DirtyDevices;
                        } else {
                            ++agentInfo->FreeDevices;
                            rackPtr->FreeBytes +=
                                device.GetBlockSize() * device.GetBlocksCount();
                        }

                        break;
                    }

                    case NProto::AGENT_STATE_WARNING: {
                        ++agentInfo->WarningDevices;
                        rackPtr->WarningBytes +=
                            device.GetBlockSize() * device.GetBlocksCount();

                        break;
                    }

                    case NProto::AGENT_STATE_UNAVAILABLE: {
                        ++agentInfo->UnavailableDevices;

                        break;
                    }

                    default: {}
                }
            }

            rackPtr->TotalBytes += device.GetBlockSize() * device.GetBlocksCount();
        }
    }

    for (auto& rack: racks) {
        SortBy(rack.AgentInfos, [] (const TRackInfo::TAgentInfo& x) {
            return x.AgentId;
        });
    }

    SortBy(racks, [] (const TRackInfo& x) {
        return x.Name;
    });

    return racks;
}

THashSet<TString> TDiskRegistryState::CollectForbiddenRacks(
    const TDiskId& diskId,
    const TDiskState& disk,
    TStringBuf callerName)
{
    THashSet<TString> forbiddenRacks;
    THashSet<TString> preferredRacks;

    if (disk.PlacementGroupId.empty()) {
        return forbiddenRacks;
    }

    auto* pg = PlacementGroups.FindPtr(disk.PlacementGroupId);
    if (!pg) {
        auto message = ReportDiskRegistryPlacementGroupNotFound(
            TStringBuilder() << callerName << ":DiskId: " << diskId
            << ", PlacementGroupId: " << disk.PlacementGroupId);

        ythrow TServiceError(E_FAIL) << message;
    }

    auto* thisDisk = CollectRacks(
        diskId,
        disk.PlacementPartitionIndex,
        pg->Config,
        &forbiddenRacks,
        &preferredRacks
    );

    if (!thisDisk) {
        auto message = ReportDiskRegistryPlacementGroupDiskNotFound(
            TStringBuilder() << callerName << ":DiskId: " << diskId
            << ", PlacementGroupId: " << disk.PlacementGroupId);

        ythrow TServiceError(E_FAIL) << message;
    }

    return forbiddenRacks;
}

NProto::TPlacementGroupConfig::TDiskInfo* TDiskRegistryState::CollectRacks(
    const TString& diskId,
    ui32 placementPartitionIndex,
    NProto::TPlacementGroupConfig& placementGroup,
    THashSet<TString>* forbiddenRacks,
    THashSet<TString>* preferredRacks)
{
    NProto::TPlacementGroupConfig::TDiskInfo* thisDisk = nullptr;

    auto isThisPartition = [&] (const auto& disk) {
        if (disk.GetDiskId() == diskId) {
            return true;
        }
        if (placementGroup.GetPlacementStrategy() ==
                NProto::PLACEMENT_STRATEGY_PARTITION &&
            disk.GetPlacementPartitionIndex() == placementPartitionIndex)
        {
            return true;
        }
        return false;
    };

    for (auto& disk: *placementGroup.MutableDisks()) {
        if (disk.GetDiskId() == diskId) {
            if (thisDisk) {
                ReportDiskRegistryDuplicateDiskInPlacementGroup(
                    TStringBuilder() << "CollectRacks:PlacementGroupId: "
                    << placementGroup.GetGroupId()
                    << ", DiskId: " << diskId);
            }

            thisDisk = &disk;
        }

        auto* racks = isThisPartition(disk) ? preferredRacks : forbiddenRacks;
        for (const auto& rack: disk.GetDeviceRacks()) {
            racks->insert(rack);
        }
    }

    return thisDisk;
}

void TDiskRegistryState::CollectForbiddenRacks(
    const NProto::TPlacementGroupConfig& placementGroup,
    THashSet<TString>* forbiddenRacks)
{
    for (auto& disk: placementGroup.GetDisks()) {
        for (const auto& rack: disk.GetDeviceRacks()) {
            forbiddenRacks->insert(rack);
        }
    }
}

THashSet<TString> TDiskRegistryState::CollectPreferredRacks(
    const TDiskId& diskId) const
{
    THashSet<TString> thisDiskRacks;

    auto diskIt = Disks.find(diskId);
    if (diskIt == Disks.end()) {
        return thisDiskRacks;
    }

    for (const TString& deviceId: diskIt->second.Devices) {
        thisDiskRacks.insert(DeviceList.FindRack(deviceId));
    }

    return thisDiskRacks;
}

NProto::TError TDiskRegistryState::ValidateDiskLocation(
    const TVector<NProto::TDeviceConfig>& diskDevices,
    const TAllocateDiskParams& params) const
{
    if (params.MediaKind != NProto::STORAGE_MEDIA_SSD_LOCAL || diskDevices.empty()) {
        return {};
    }

    if (!params.AgentIds.empty()) {
        const THashSet<TString> agents(
            params.AgentIds.begin(),
            params.AgentIds.end());

        for (const auto& device: diskDevices) {
            if (!agents.contains(device.GetAgentId())) {
                return MakeError(E_ARGUMENT, TStringBuilder() <<
                    "disk " << params.DiskId.Quote() << " already allocated at "
                        << device.GetAgentId());
            }
        }
    }

    return {};
}

TResultOrError<TDeviceList::TAllocationQuery> TDiskRegistryState::PrepareAllocationQuery(
    ui64 blocksToAllocate,
    const TDiskPlacementInfo& placementInfo,
    const TVector<NProto::TDeviceConfig>& diskDevices,
    const TAllocateDiskParams& params)
{
    THashSet<TString> forbiddenDiskRacks;
    THashSet<TString> preferredDiskRacks;

    if (placementInfo.PlacementGroupId) {
        auto& group = PlacementGroups[placementInfo.PlacementGroupId];

        CollectRacks(
            params.DiskId,
            placementInfo.PlacementPartitionIndex,
            group.Config,
            &forbiddenDiskRacks,
            &preferredDiskRacks);
    } else {
        preferredDiskRacks = CollectPreferredRacks(params.DiskId);
    }

    THashSet<ui32> nodeIds;
    TVector<TString> unknownAgents;

    for (const auto& id: params.AgentIds) {
        const ui32 nodeId = AgentList.FindNodeId(id);
        if (!nodeId) {
            unknownAgents.push_back(id);
        } else {
            nodeIds.insert(nodeId);
        }
    }

    if (!unknownAgents.empty()) {
        return MakeError(E_ARGUMENT, TStringBuilder() <<
            "unknown agents: " << JoinSeq(", ", unknownAgents));
    }

    NProto::EDevicePoolKind poolKind = params.PoolName.empty()
        ? NProto::DEVICE_POOL_KIND_DEFAULT
        : NProto::DEVICE_POOL_KIND_GLOBAL;

    if (params.MediaKind == NProto::STORAGE_MEDIA_SSD_LOCAL) {
        poolKind = NProto::DEVICE_POOL_KIND_LOCAL;
    }

    if (poolKind == NProto::DEVICE_POOL_KIND_LOCAL && !diskDevices.empty()) {
        const auto nodeId = diskDevices[0].GetNodeId();

        if (!nodeIds.empty() && !nodeIds.contains(nodeId)) {
            return MakeError(E_ARGUMENT, TStringBuilder()
                << "disk " << params.DiskId << " already allocated on "
                << diskDevices[0].GetAgentId());
        }

        nodeIds = { nodeId };
    }

    return TDeviceList::TAllocationQuery {
        .ForbiddenRacks = std::move(forbiddenDiskRacks),
        .PreferredRacks = std::move(preferredDiskRacks),
        .LogicalBlockSize = params.BlockSize,
        .BlockCount = blocksToAllocate,
        .PoolName = params.PoolName,
        .PoolKind = poolKind,
        .NodeIds = std::move(nodeIds),
    };
}

NProto::TError TDiskRegistryState::ValidateAllocateDiskParams(
    const TDiskState& disk,
    const TAllocateDiskParams& params) const
{
    if (disk.ReplicaCount && !params.ReplicaCount) {
        return MakeError(
            E_INVALID_STATE,
            "attempt to reallocate mirrored disk as nonrepl");
    }

    if (params.ReplicaCount && !disk.ReplicaCount && disk.Devices) {
        return MakeError(
            E_INVALID_STATE,
            "attempt to reallocate nonrepl disk as mirrored");
    }

    if (disk.LogicalBlockSize && disk.LogicalBlockSize != params.BlockSize) {
        return MakeError(
            E_ARGUMENT,
            TStringBuilder() << "attempt to change LogicalBlockSize: "
                << disk.LogicalBlockSize << " -> " << params.BlockSize);
    }

    return {};
}

NProto::TError TDiskRegistryState::AllocateDisk(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TAllocateDiskParams& params,
    TAllocateDiskResult* result)
{
    auto& disk = Disks[params.DiskId];
    auto error = ValidateAllocateDiskParams(disk, params);

    if (HasError(error)) {
        if (disk.Devices.empty() && !disk.ReplicaCount) {
            Disks.erase(params.DiskId);
            AddToBrokenDisks(now, db, params.DiskId);
        }

        return error;
    }

    if (params.ReplicaCount) {
        return AllocateMirroredDisk(now, db, params, disk, result);
    }

    return AllocateSimpleDisk(now, db, params, disk, result);
}

bool TDiskRegistryState::IsMirroredDiskAlreadyAllocated(
    const TAllocateDiskParams& params) const
{
    const auto replicaId = GetReplicaDiskId(params.DiskId, 0);
    const auto* replica = Disks.FindPtr(replicaId);
    if (!replica) {
        return false;
    }

    ui64 size = 0;
    for (const auto& id: replica->Devices) {
        const auto* device = DeviceList.FindDevice(id);
        if (device) {
            size += device->GetBlockSize() * device->GetBlocksCount();
        }
    }

    return size >= params.BlocksCount * params.BlockSize;
}

void TDiskRegistryState::CleanupMirroredDisk(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TAllocateDiskParams& params)
{
    const auto& diskId = params.DiskId;
    const auto groupId = GetMirroredDiskGroupId(diskId);

    Disks.erase(diskId);

    for (ui32 i = 0; i < params.ReplicaCount + 1; ++i) {
        const auto replicaId = GetReplicaDiskId(diskId, i);
        auto* replica = Disks.FindPtr(replicaId);
        if (replica) {
            DeallocateSimpleDisk(db, replicaId, *replica);
        }
    }

    TVector<TString> affectedDisks;
    DestroyPlacementGroup(db, groupId, affectedDisks);

    if (affectedDisks.size()) {
        ReportMirroredDiskAllocationPlacementGroupCleanupFailure(
            TStringBuilder()
                << "AllocateMirroredDisk:PlacementGroupCleanupFailure:DiskId: "
                << diskId);
    }

    AddToBrokenDisks(now, db, diskId);
}

void TDiskRegistryState::UpdateReplicaTable(
    const TDiskId& diskId,
    const TAllocateDiskResult& r)
{
    TVector<TDeviceId> deviceIds;
    for (const auto& d: r.Devices) {
        deviceIds.push_back(d.GetDeviceUUID());
    }

    ReplicaTable.UpdateReplica(diskId, 0, deviceIds);

    for (ui32 i = 0; i < r.Replicas.size(); ++i) {
        deviceIds.clear();
        for (const auto& d: r.Replicas[i]) {
            deviceIds.push_back(d.GetDeviceUUID());
        }

        ReplicaTable.UpdateReplica(diskId, i + 1, deviceIds);
    }

    for (const auto& deviceId: r.DeviceReplacementIds) {
        ReplicaTable.MarkReplacementDevice(diskId, deviceId, true);
    }
}

NProto::TError TDiskRegistryState::CreateMirroredDiskPlacementGroup(
    TDiskRegistryDatabase& db,
    const TDiskId& diskId)
{
    return CreatePlacementGroup(
        db,
        GetMirroredDiskGroupId(diskId),
        NProto::PLACEMENT_STRATEGY_SPREAD,
        0);
}

NProto::TError TDiskRegistryState::AllocateDiskReplica(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TAllocateDiskParams& params,
    ui32 index,
    TAllocateDiskResult* result)
{
    TAllocateDiskParams subParams = params;
    subParams.ReplicaCount = 0;
    subParams.DiskId = GetReplicaDiskId(params.DiskId, index);
    subParams.PlacementGroupId = GetMirroredDiskGroupId(params.DiskId);
    subParams.MasterDiskId = params.DiskId;

    return AllocateSimpleDisk(
        now,
        db,
        subParams,
        Disks[subParams.DiskId],
        result);
}

NProto::TError TDiskRegistryState::AllocateDiskReplicas(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TAllocateDiskParams& params,
    TAllocateDiskResult* result)
{
    for (ui32 i = 0; i < params.ReplicaCount + 1; ++i) {
        TAllocateDiskResult subResult;
        auto error = AllocateDiskReplica(now, db, params, i, &subResult);

        if (HasError(error)) {
            return error;
        }

        if (i) {
            result->Replicas.push_back(std::move(subResult.Devices));
        } else {
            result->Devices = std::move(subResult.Devices);
        }
        result->Migrations.insert(
            result->Migrations.end(),
            std::make_move_iterator(subResult.Migrations.begin()),
            std::make_move_iterator(subResult.Migrations.end()));
    }

    return {};
}

NProto::TError TDiskRegistryState::AllocateMirroredDisk(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TAllocateDiskParams& params,
    TDiskState& disk,
    TAllocateDiskResult* result)
{
    const bool isNewDisk = disk.Devices.empty() && !disk.ReplicaCount;

    auto onError = [&] {
        if (isNewDisk) {
            CleanupMirroredDisk(now, db, params);
        }
    };

    const ui32 code = IsMirroredDiskAlreadyAllocated(params)
        ? S_ALREADY
        : S_OK;

    if (isNewDisk) {
        auto error = CreateMirroredDiskPlacementGroup(db, params.DiskId);
        if (HasError(error)) {
            onError();

            return error;
        }
    }

    if (auto error = AllocateDiskReplicas(now, db, params, result); HasError(error)) {
        if (!isNewDisk) {
            // TODO (NBS-3419):
            // support automatic cleanup after a failed resize
            ReportMirroredDiskAllocationCleanupFailure(TStringBuilder()
                << "AllocateMirroredDisk:ResizeCleanupFailure:DiskId: "
                << params.DiskId);
        }

        onError();

        return error;
    }

    result->DeviceReplacementIds = disk.DeviceReplacementIds;

    disk.CloudId = params.CloudId;
    disk.FolderId = params.FolderId;
    disk.LogicalBlockSize = params.BlockSize;
    disk.StateTs = now;
    disk.ReplicaCount = params.ReplicaCount;
    disk.MediaKind = params.MediaKind;
    db.UpdateDisk(BuildDiskConfig(params.DiskId, disk));

    UpdateReplicaTable(params.DiskId, *result);

    return MakeError(code);
}

NProto::TError TDiskRegistryState::CheckDiskPlacementInfo(
    const TDiskPlacementInfo& info) const
{
    auto* g = PlacementGroups.FindPtr(info.PlacementGroupId);
    if (!g) {
        return MakeError(E_NOT_FOUND, TStringBuilder()
            << "placement group " << info.PlacementGroupId << " not found");
    }

    if (info.PlacementPartitionIndex && !PlacementGroupMustHavePartitions(g->Config)) {
        return MakeError(E_ARGUMENT, TStringBuilder()
            << "placement partition shouldn't be specified for placement group "
            << info.PlacementGroupId.Quote());
    }

    if (!PartitionSuitsPlacementGroup(g->Config, info.PlacementPartitionIndex)) {
        return MakeError(
            E_NOT_FOUND,
            TStringBuilder()
                << "placement partition index "
                << info.PlacementPartitionIndex
                << " not found in placement group "
                << info.PlacementGroupId.Quote());
    }

    return {};
}

NProto::TError TDiskRegistryState::CheckPlacementGroupCapacity(
    const TString& groupId) const
{
    auto* group = PlacementGroups.FindPtr(groupId);
    if (!group) {
        return MakeError(E_NOT_FOUND, TStringBuilder()
            << "placement group " << groupId << " not found");
    }

    const auto& config = group->Config;

    if (config.DisksSize() < GetMaxDisksInPlacementGroup(*StorageConfig, config)) {
        return {};
    }

    ui32 flags = 0;
    SetProtoFlag(flags, NProto::EF_SILENT);

    return MakeError(E_BS_RESOURCE_EXHAUSTED, TStringBuilder() <<
        "max disk count in group exceeded, max: "
        << GetMaxDisksInPlacementGroup(*StorageConfig, config),
        flags);
}

void TDiskRegistryState::UpdateDiskPlacementInfo(
    TDiskRegistryDatabase& db,
    const TDiskId& diskId,
    const TDiskPlacementInfo& placementInfo)
{
    if (!placementInfo.PlacementGroupId) {
        return;
    }

    auto& config = PlacementGroups[placementInfo.PlacementGroupId].Config;

    NProto::TPlacementGroupConfig::TDiskInfo* thisDisk = nullptr;

    for (auto& diskInfo: *config.MutableDisks()) {
        if (diskInfo.GetDiskId() == diskId) {
            thisDisk = &diskInfo;
            break;
        }
    }

    if (!thisDisk) {
        thisDisk = config.AddDisks();
        thisDisk->SetDiskId(diskId);
    }

    auto& disk = Disks[diskId];

    disk.PlacementGroupId = placementInfo.PlacementGroupId;
    disk.PlacementPartitionIndex = placementInfo.PlacementPartitionIndex;
    RebuildDiskPlacementInfo(disk, thisDisk);

    config.SetConfigVersion(config.GetConfigVersion() + 1);
    db.UpdatePlacementGroup(config);
}

auto TDiskRegistryState::CreateDiskPlacementInfo(
    const TDiskState& disk,
    const TAllocateDiskParams& params) const -> TDiskPlacementInfo
{
    return {
        disk.Devices && disk.PlacementGroupId
            ? disk.PlacementGroupId
            : params.PlacementGroupId,
        disk.Devices && disk.PlacementPartitionIndex
            ? disk.PlacementPartitionIndex
            : params.PlacementPartitionIndex
    };
}

NProto::TError TDiskRegistryState::AllocateSimpleDisk(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TAllocateDiskParams& params,
    TDiskState& disk,
    TAllocateDiskResult* result)
{
    STORAGE_VERIFY(
        !disk.ReplicaCount && !params.ReplicaCount,
        TWellKnownEntityTypes::DISK,
        params.DiskId);

    auto onError = [&] {
        const bool isNewDisk = disk.Devices.empty();

        if (isNewDisk) {
            Disks.erase(params.DiskId);

            if (!params.MasterDiskId) {
                // failed to allocate storage for the new volume, need to
                // destroy this volume
                AddToBrokenDisks(now, db, params.DiskId);
            }
        }
    };

    if (!disk.StateTs) {
        disk.StateTs = now;
    }

    result->IOModeTs = disk.StateTs;
    result->IOMode = disk.State < NProto::DISK_STATE_ERROR
        ? NProto::VOLUME_IO_OK
        : NProto::VOLUME_IO_ERROR_READ_ONLY;

    result->MuteIOErrors =
        disk.State >= NProto::DISK_STATE_TEMPORARILY_UNAVAILABLE;

    const TDiskPlacementInfo placementInfo = CreateDiskPlacementInfo(disk, params);

    if (placementInfo.PlacementGroupId) {
        auto error = CheckDiskPlacementInfo(placementInfo);
        if (HasError(error)) {
            onError();
            return error;
        }
    }

    auto& output = result->Devices;

    if (auto error = GetDiskMigrations(disk, result->Migrations); HasError(error)) {
        onError();

        return error;
    }

    if (auto error = GetDiskDevices(params.DiskId, disk, output); HasError(error)) {
        onError();

        return error;
    }

    if (auto error = ValidateDiskLocation(output, params); HasError(error)) {
        onError();

        return error;
    }

    ui64 currentSize = 0;
    for (const auto& d: output) {
        currentSize += d.GetBlockSize() * d.GetBlocksCount();
    }

    const ui64 requestedSize = params.BlockSize * params.BlocksCount;

    if (requestedSize <= currentSize) {
        if (disk.CloudId.empty() && !params.CloudId.empty()) {
            disk.CloudId = params.CloudId;
            disk.FolderId = params.FolderId;
            db.UpdateDisk(BuildDiskConfig(params.DiskId, disk));
        }

        return MakeError(S_ALREADY, TStringBuilder() <<
            "disk " << params.DiskId.Quote() << " already exists");
    }

    const ui64 blocksToAllocate =
        ceil(double(requestedSize - currentSize) / params.BlockSize);

    if (output.empty() && placementInfo.PlacementGroupId) {
        auto error = CheckPlacementGroupCapacity(placementInfo.PlacementGroupId);
        if (HasError(error)) {
            onError();
            return error;
        }
    }

    auto [query, error] = PrepareAllocationQuery(
        blocksToAllocate,
        placementInfo,
        output,
        params);

    if (HasError(error)) {
        onError();

        return error;
    }

    auto allocatedDevices = DeviceList.AllocateDevices(params.DiskId, query);

    if (!allocatedDevices) {
        onError();

        return MakeError(E_BS_DISK_ALLOCATION_FAILED, TStringBuilder() <<
            "can't allocate disk with " << blocksToAllocate << " blocks x " <<
            params.BlockSize << " bytes");
    }

    for (const auto& device: allocatedDevices) {
        disk.Devices.push_back(device.GetDeviceUUID());
    }

    output.insert(
        output.end(),
        std::make_move_iterator(allocatedDevices.begin()),
        std::make_move_iterator(allocatedDevices.end()));

    disk.LogicalBlockSize = params.BlockSize;
    disk.CloudId = params.CloudId;
    disk.FolderId = params.FolderId;
    disk.MasterDiskId = params.MasterDiskId;
    disk.MediaKind = params.MediaKind;

    db.UpdateDisk(BuildDiskConfig(params.DiskId, disk));

    UpdateDiskPlacementInfo(db, params.DiskId, placementInfo);

    AllowNotifications(params.DiskId, disk);

    return {};
}

NProto::TError TDiskRegistryState::DeallocateDisk(
    TDiskRegistryDatabase& db,
    const TString& diskId)
{
    auto* disk = Disks.FindPtr(diskId);
    if (!disk) {
        return MakeError(S_ALREADY, TStringBuilder() <<
            "disk " << diskId.Quote() << " not found");
    }

    if (!IsReadyForCleanup(diskId)) {
        auto message = ReportNrdDestructionError(TStringBuilder()
            << "attempting to clean up unmarked disk " << diskId.Quote());

        return MakeError(E_INVALID_STATE, std::move(message));
    }

    if (disk->ReplicaCount) {
        const TString groupId = GetMirroredDiskGroupId(diskId);
        TVector<TString> affectedDisks;
        auto error = DestroyPlacementGroup(db, groupId, affectedDisks);
        if (HasError(error)) {
            return error;
        }

        for (const auto& affectedDiskId: affectedDisks) {
            Y_VERIFY_DEBUG(affectedDiskId.StartsWith(diskId + "/"));
            PendingCleanup.Insert(
                diskId,
                DeallocateSimpleDisk(db, affectedDiskId, "DeallocateDisk:Replica")
            );
        }

        DeleteDisk(db, diskId);
        ReplicaTable.RemoveMirroredDisk(diskId);

        return {};
    }

    PendingCleanup.Insert(
        diskId,
        DeallocateSimpleDisk(db, diskId, *disk)
    );

    return {};
}

bool TDiskRegistryState::HasPendingCleanup(const TDiskId& diskId) const
{
    return PendingCleanup.Contains(diskId);
}

auto TDiskRegistryState::DeallocateSimpleDisk(
    TDiskRegistryDatabase& db,
    const TString& diskId,
    const TString& parentMethodName) -> TVector<TDeviceId>
{
    auto* disk = Disks.FindPtr(diskId);
    if (!disk) {
        ReportDiskRegistryDiskNotFound(
            TStringBuilder() << parentMethodName << ":DiskId: "
            << diskId);

        return {};
    }

    return DeallocateSimpleDisk(db, diskId, *disk);
}

auto TDiskRegistryState::DeallocateSimpleDisk(
    TDiskRegistryDatabase& db,
    const TString& diskId,
    TDiskState& disk) -> TVector<TDeviceId>
{
    Y_VERIFY(disk.ReplicaCount == 0);

    TVector<TDeviceId> dirtyDevices;

    for (const auto& uuid: disk.Devices) {
        if (DeviceList.ReleaseDevice(uuid)) {
            dirtyDevices.push_back(uuid);
        }
    }

    if (disk.PlacementGroupId) {
        auto* pg = PlacementGroups.FindPtr(disk.PlacementGroupId);

        if (pg) {
            auto& config = pg->Config;

            auto end = std::remove_if(
                config.MutableDisks()->begin(),
                config.MutableDisks()->end(),
                [&] (const NProto::TPlacementGroupConfig::TDiskInfo& d) {
                    return diskId == d.GetDiskId();
                }
            );

            while (config.MutableDisks()->end() > end) {
                config.MutableDisks()->RemoveLast();
            }

            config.SetConfigVersion(config.GetConfigVersion() + 1);
            db.UpdatePlacementGroup(config);
        } else {
            ReportDiskRegistryPlacementGroupNotFound(
                TStringBuilder() << "DeallocateDisk:DiskId: " << diskId
                << ", PlacementGroupId: " << disk.PlacementGroupId);
        }
    }

    for (const auto& [targetId, sourceId]: disk.MigrationTarget2Source) {
        Y_UNUSED(sourceId);

        if (DeviceList.ReleaseDevice(targetId)) {
            dirtyDevices.push_back(targetId);
        }
    }

    for (const auto& [uuid, seqNo]: disk.FinishedMigrations) {
        Y_UNUSED(seqNo);

        if (DeviceList.ReleaseDevice(uuid)) {
            dirtyDevices.push_back(uuid);
        }
    }

    for (const auto& uuid: dirtyDevices) {
        db.UpdateDirtyDevice(uuid, diskId);
    }

    DeleteAllDeviceMigrations(diskId);
    DeleteDisk(db, diskId);

    return dirtyDevices;
}

void TDiskRegistryState::DeleteDisk(
    TDiskRegistryDatabase& db,
    const TString& diskId)
{
    Disks.erase(diskId);
    DisksToCleanup.erase(diskId);

    NotificationSystem.DeleteDisk(db, diskId);

    db.DeleteDisk(diskId);
    db.DeleteDiskToCleanup(diskId);
}

void TDiskRegistryState::AddToBrokenDisks(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TString& diskId)
{
    TBrokenDiskInfo brokenDiskInfo{
        diskId,
        now + StorageConfig->GetBrokenDiskDestructionDelay()
    };
    db.AddBrokenDisk(brokenDiskInfo);
    BrokenDisks.push_back(brokenDiskInfo);
}

NProto::TDeviceConfig TDiskRegistryState::GetDevice(const TString& id) const
{
    const auto* device = DeviceList.FindDevice(id);

    if (device) {
        return *device;
    }

    return {};
}

TVector<NProto::TDeviceConfig> TDiskRegistryState::FindDevices(
    const TString& agentId,
    const TString& path) const
{
    const auto* agent = AgentList.FindAgent(agentId);
    if (!agent) {
        return {};
    }

    TVector<NProto::TDeviceConfig> devices;
    for (const auto& device: agent->GetDevices()) {
        if (device.GetDeviceName() == path) {
            devices.push_back(device);
        }
    }
    return devices;
}

TResultOrError<NProto::TDeviceConfig> TDiskRegistryState::FindDevice(
    const NProto::TDeviceConfig& deviceConfig) const
{
    const auto& deviceId = deviceConfig.GetDeviceUUID();
    if (deviceId) {
        const auto* device = DeviceList.FindDevice(deviceId);
        if (!device) {
            return MakeError(
                E_NOT_FOUND,
                TStringBuilder() << "device with id " << deviceId
                    << " not found");
        }

        return *device;
    }

    auto devices = FindDevices(
        deviceConfig.GetAgentId(),
        deviceConfig.GetDeviceName());

    if (devices.size() == 0) {
        return MakeError(
            E_NOT_FOUND,
            TStringBuilder() << "device with AgentId="
                << deviceConfig.GetAgentId()
                << " and DeviceName=" << deviceConfig.GetDeviceName()
                << " not found");
    }

    if (devices.size() > 1) {
        return MakeError(
            E_ARGUMENT,
            TStringBuilder() << "too many devices with AgentId="
                << deviceConfig.GetAgentId()
                << " and DeviceName=" << deviceConfig.GetDeviceName()
                << ": " << devices.size());
    }

    return devices.front();
}


TVector<TString> TDiskRegistryState::GetDeviceIds(
    const TString& agentId,
    const TString& path) const
{
    auto devices = FindDevices(agentId, path);
    TVector<TString> deviceIds;
    deviceIds.reserve(devices.size());
    for (auto& device: devices) {
        deviceIds.push_back(*std::move(device.MutableDeviceUUID()));
    }
    return deviceIds;
}

NProto::EDeviceState TDiskRegistryState::GetDeviceState(
    const TString& deviceId) const
{
    return DeviceList.GetDeviceState(deviceId);
}

NProto::TError TDiskRegistryState::GetDependentDisks(
    const TString& agentId,
    const TString& path,
    TVector<TDiskId>* diskIds) const
{
    auto* agent = AgentList.FindAgent(agentId);
    if (!agent) {
        return MakeError(E_NOT_FOUND, agentId);
    }

    for (const auto& d: agent->GetDevices()) {
        if (path && d.GetDeviceName() != path) {
            continue;
        }

        auto diskId = FindDisk(d.GetDeviceUUID());

        if (!diskId) {
            continue;
        }

        // linear search on every iteration is ok here, diskIds size is small
        if (Find(*diskIds, diskId) != diskIds->end()) {
            continue;
        }

        diskIds->push_back(std::move(diskId));
    }

    return {};
}

NProto::TError TDiskRegistryState::GetDiskDevices(
    const TDiskId& diskId,
    TVector<NProto::TDeviceConfig>& devices) const
{
    auto it = Disks.find(diskId);

    if (it == Disks.end()) {
        return MakeError(E_NOT_FOUND, TStringBuilder() <<
            "disk " << diskId.Quote() << " not found");
    }

    return GetDiskDevices(diskId, it->second, devices);
}

NProto::TError TDiskRegistryState::GetDiskDevices(
    const TDiskId& diskId,
    const TDiskState& disk,
    TVector<NProto::TDeviceConfig>& devices) const
{
    auto* overrides = DeviceOverrides.FindPtr(diskId);

    for (const auto& uuid: disk.Devices) {
        const auto* device = DeviceList.FindDevice(uuid);

        if (!device) {
            return MakeError(E_NOT_FOUND, TStringBuilder() <<
                "device " << uuid.Quote() << " not found");
        }

        devices.emplace_back(*device);
        if (overrides) {
            auto* blocksCount = overrides->Device2BlockCount.FindPtr(uuid);
            if (blocksCount) {
                devices.back().SetBlocksCount(*blocksCount);
            }
        }
    }

    return {};
}

NProto::TError TDiskRegistryState::GetDiskMigrations(
    const TDiskId& diskId,
    TVector<NProto::TDeviceMigration>& migrations) const
{
    auto it = Disks.find(diskId);

    if (it == Disks.end()) {
        return MakeError(E_NOT_FOUND, TStringBuilder() <<
            "disk " << diskId.Quote() << " not found");
    }

    return GetDiskMigrations(it->second, migrations);
}

NProto::TError TDiskRegistryState::GetDiskMigrations(
    const TDiskState& disk,
    TVector<NProto::TDeviceMigration>& migrations) const
{
    ui32 oldSize = migrations.size();
    migrations.reserve(oldSize + disk.MigrationTarget2Source.size());

    for (const auto& [targetId, sourceId]: disk.MigrationTarget2Source) {
        NProto::TDeviceMigration m;
        m.SetSourceDeviceId(sourceId);
        *m.MutableTargetDevice() = GetDevice(targetId);

        const auto& uuid = m.GetTargetDevice().GetDeviceUUID();

        if (uuid.empty()) {
            return MakeError(E_NOT_FOUND, TStringBuilder() <<
                "device " << uuid.Quote() << " not found");
        }

        migrations.push_back(std::move(m));
    }

    // for convenience
    // the logic in other components should not have critical dependencies on
    // the order of migrations
    auto b = migrations.begin();
    std::advance(b, oldSize);
    SortBy(b, migrations.end(), [] (const NProto::TDeviceMigration& m) {
        return m.GetSourceDeviceId();
    });

    return {};
}

NProto::TError TDiskRegistryState::FillAllDiskDevices(
    const TDiskId& diskId,
    const TDiskState& disk,
    TDiskInfo& diskInfo) const
{
    NProto::TError error;

    if (disk.ReplicaCount) {
        error = GetDiskDevices(GetReplicaDiskId(diskId, 0), diskInfo.Devices);
        diskInfo.Replicas.resize(disk.ReplicaCount);
        for (ui32 i = 0; i < disk.ReplicaCount; ++i) {
            if (!HasError(error)) {
                error = GetDiskDevices(
                    GetReplicaDiskId(diskId, i + 1),
                    diskInfo.Replicas[i]);
            }
        }
    } else {
        error = GetDiskDevices(diskId, disk, diskInfo.Devices);
    }

    if (!HasError(error)) {
        if (disk.ReplicaCount) {
            error = GetDiskMigrations(
                GetReplicaDiskId(diskId, 0),
                diskInfo.Migrations);
            for (ui32 i = 0; i < disk.ReplicaCount; ++i) {
                if (!HasError(error)) {
                    error = GetDiskMigrations(
                        GetReplicaDiskId(diskId, i + 1),
                        diskInfo.Migrations);
                }
            }
        } else {
            error = GetDiskMigrations(disk, diskInfo.Migrations);
        }
    }

    return error;
}

NProto::EDiskState TDiskRegistryState::GetDiskState(const TDiskId& diskId) const
{
    auto* disk = Disks.FindPtr(diskId);
    if (!disk) {
        return NProto::DISK_STATE_ERROR;
    }

    return disk->State;
}

NProto::TError TDiskRegistryState::GetDiskInfo(
    const TString& diskId,
    TDiskInfo& diskInfo) const
{
    auto it = Disks.find(diskId);

    if (it == Disks.end()) {
        return MakeError(E_NOT_FOUND, TStringBuilder() <<
            "disk " << diskId.Quote() << " not found");
    }

    const auto& disk = it->second;

    diskInfo.CloudId = disk.CloudId;
    diskInfo.FolderId = disk.FolderId;
    diskInfo.UserId = disk.UserId;
    diskInfo.LogicalBlockSize = disk.LogicalBlockSize;
    diskInfo.State = disk.State;
    diskInfo.StateTs = disk.StateTs;
    diskInfo.PlacementGroupId = disk.PlacementGroupId;
    diskInfo.PlacementPartitionIndex = disk.PlacementPartitionIndex;
    diskInfo.FinishedMigrations = disk.FinishedMigrations;
    diskInfo.DeviceReplacementIds = disk.DeviceReplacementIds;
    diskInfo.MediaKind = disk.MediaKind;
    diskInfo.MasterDiskId = disk.MasterDiskId;

    auto error = FillAllDiskDevices(diskId, disk, diskInfo);

    if (error.GetCode() == E_NOT_FOUND) {
        return MakeError(E_INVALID_STATE, error.GetMessage());
    }

    return error;
}

bool TDiskRegistryState::FilterDevicesAtUnavailableAgents(TDiskInfo& diskInfo) const
{
    auto isUnavailable = [&] (const auto& d) {
        const auto* agent = AgentList.FindAgent(d.GetAgentId());

        return !agent || agent->GetState() == NProto::AGENT_STATE_UNAVAILABLE;
    };

    EraseIf(diskInfo.Devices, isUnavailable);
    EraseIf(diskInfo.Migrations, [&] (const auto& d) {
        return isUnavailable(d.GetTargetDevice());
    });

    return diskInfo.Devices.size() || diskInfo.Migrations.size();
}

NProto::TError TDiskRegistryState::StartAcquireDisk(
    const TString& diskId,
    TDiskInfo& diskInfo)
{
    auto it = Disks.find(diskId);

    if (it == Disks.end()) {
        return MakeError(E_NOT_FOUND, TStringBuilder() <<
            "disk " << diskId.Quote() << " not found");
    }

    auto& disk = it->second;

    if (disk.AcquireInProgress) {
        return MakeError(E_REJECTED, TStringBuilder() <<
            "disk " << diskId.Quote() << " acquire in progress");
    }

    auto error = FillAllDiskDevices(diskId, disk, diskInfo);

    if (HasError(error)) {
        return error;
    }

    disk.AcquireInProgress = true;

    diskInfo.LogicalBlockSize = disk.LogicalBlockSize;

    return {};
}

void TDiskRegistryState::FinishAcquireDisk(const TString& diskId)
{
    auto it = Disks.find(diskId);

    if (it == Disks.end()) {
        return;
    }

    auto& disk = it->second;

    disk.AcquireInProgress = false;
}

bool TDiskRegistryState::IsAcquireInProgress(const TString& diskId) const
{
    auto it = Disks.find(diskId);

    if (it == Disks.end()) {
        return false;
    }

    return it->second.AcquireInProgress;
}

ui32 TDiskRegistryState::GetConfigVersion() const
{
    return CurrentConfig.GetVersion();
}

NProto::TError TDiskRegistryState::UpdateConfig(
    TDiskRegistryDatabase& db,
    NProto::TDiskRegistryConfig newConfig,
    bool ignoreVersion,
    TVector<TString>& affectedDisks)
{
    if (!ignoreVersion && newConfig.GetVersion() != CurrentConfig.GetVersion()) {
        return MakeError(E_ABORTED, "Wrong config version");
    }

    for (const auto& pool: newConfig.GetDevicePoolConfigs()) {
        if (pool.GetName().empty()
                && pool.GetKind() != NProto::DEVICE_POOL_KIND_DEFAULT)
        {
            return MakeError(E_ARGUMENT, "non default pool with empty name");
        }

        if (!pool.GetName().empty()
                && pool.GetKind() == NProto::DEVICE_POOL_KIND_DEFAULT)
        {
            return MakeError(E_ARGUMENT, "default pool with non empty name");
        }
    }

    THashSet<TString> allDevices;
    TKnownAgents newKnownAgents;

    for (const auto& agent: newConfig.GetKnownAgents()) {
        if (newKnownAgents.contains(agent.GetAgentId())) {
            return MakeError(E_ARGUMENT, "bad config");
        }

        TKnownAgent& knownAgent = newKnownAgents[agent.GetAgentId()];

        for (const auto& device: agent.GetDevices()) {
            knownAgent.Devices.emplace(device.GetDeviceUUID(), device);
            auto [_, ok] = allDevices.insert(device.GetDeviceUUID());
            if (!ok) {
                return MakeError(E_ARGUMENT, "bad config");
            }
        }
    }

    TVector<TString> removedDevices;
    THashSet<TString> updatedAgents;

    for (const auto& [id, knownAgent]: KnownAgents) {
        for (const auto& [uuid, _]: knownAgent.Devices) {
            if (!allDevices.contains(uuid)) {
                removedDevices.push_back(uuid);
                updatedAgents.insert(id);
            }
        }

        if (!newKnownAgents.contains(id)) {
            updatedAgents.insert(id);
        }
    }

    THashSet<TString> diskIds;

    for (const auto& uuid: removedDevices) {
        auto diskId = DeviceList.FindDiskId(uuid);
        if (diskId) {
            diskIds.emplace(std::move(diskId));
        }
    }

    affectedDisks.assign(
        std::make_move_iterator(diskIds.begin()),
        std::make_move_iterator(diskIds.end()));

    Sort(affectedDisks);

    if (!affectedDisks.empty()) {
        return MakeError(E_INVALID_STATE, "Destructive configuration change");
    }

    if (Counters) {
        for (const auto& pool: newConfig.GetDevicePoolConfigs()) {
            SelfCounters.RegisterPool(pool.GetName(), Counters);
        }
    }

    newConfig.SetVersion(CurrentConfig.GetVersion() + 1);
    ProcessConfig(newConfig);

    TVector<TDiskId> disksToReallocate;
    for (const auto& agentId: updatedAgents) {
        const auto* agent = AgentList.FindAgent(agentId);
        if (agent) {
            const auto ts = TInstant::MicroSeconds(agent->GetStateTs());
            auto config = *agent;

            auto error = RegisterAgent(
                db,
                config,
                ts,
                &affectedDisks,
                &disksToReallocate);

            STORAGE_VERIFY_C(
                !HasError(error),
                TWellKnownEntityTypes::AGENT,
                config.GetAgentId(),
                "agent update failure: " << FormatError(error) << ". Config: "
                    << config);
        }
    }

    db.WriteDiskRegistryConfig(newConfig);
    CurrentConfig = std::move(newConfig);

    return {};
}

void TDiskRegistryState::RemoveAgent(
    TDiskRegistryDatabase& db,
    const NProto::TAgentConfig& agent)
{
    const auto nodeId = agent.GetNodeId();
    const auto agentId = agent.GetAgentId();

    DeviceList.RemoveDevices(agent);
    AgentList.RemoveAgent(nodeId);

    db.DeleteOldAgent(nodeId);
    db.DeleteAgent(agentId);
}

template <typename T>
bool TDiskRegistryState::RemoveAgent(
    TDiskRegistryDatabase& db,
    const T& id)
{
    auto* agent = AgentList.FindAgent(id);
    if (!agent) {
        return false;
    }

    RemoveAgent(db, *agent);

    return true;
}

void TDiskRegistryState::ProcessConfig(const NProto::TDiskRegistryConfig& config)
{
    TKnownAgents newKnownAgents;

    for (const auto& agent: config.GetKnownAgents()) {
        TKnownAgent& knownAgent = newKnownAgents[agent.GetAgentId()];

        for (const auto& device: agent.GetDevices()) {
            knownAgent.Devices.emplace(device.GetDeviceUUID(), device);
        }
    }

    newKnownAgents.swap(KnownAgents);

    TDeviceOverrides newDeviceOverrides;

    for (const auto& deviceOverride: config.GetDeviceOverrides()) {
        auto& diskOverrides = newDeviceOverrides[deviceOverride.GetDiskId()];
        diskOverrides.Device2BlockCount[deviceOverride.GetDevice()] =
            deviceOverride.GetBlocksCount();
    }

    newDeviceOverrides.swap(DeviceOverrides);

    DevicePoolConfigs = CreateDevicePoolConfigs(config, *StorageConfig);
}

const NProto::TDiskRegistryConfig& TDiskRegistryState::GetConfig() const
{
    return CurrentConfig;
}

ui32 TDiskRegistryState::GetDiskCount() const
{
    return Disks.size();
}

TVector<TString> TDiskRegistryState::GetDiskIds() const
{
    TVector<TString> ids(Reserve(Disks.size()));

    for (auto& kv: Disks) {
        ids.push_back(kv.first);
    }

    Sort(ids);

    return ids;
}

TVector<TString> TDiskRegistryState::GetMasterDiskIds() const
{
    TVector<TString> ids(Reserve(Disks.size()));

    for (auto& kv: Disks) {
        if (!kv.second.MasterDiskId) {
            ids.push_back(kv.first);
        }
    }

    Sort(ids);

    return ids;
}

TVector<TString> TDiskRegistryState::GetMirroredDiskIds() const
{
    TVector<TString> ids;

    for (auto& kv: Disks) {
        if (kv.second.ReplicaCount) {
            ids.push_back(kv.first);
        }
    }

    Sort(ids);

    return ids;
}

bool TDiskRegistryState::IsMasterDisk(const TString& diskId) const
{
    const auto* disk = Disks.FindPtr(diskId);
    return disk && disk->ReplicaCount;
}

TVector<NProto::TDeviceConfig> TDiskRegistryState::GetBrokenDevices() const
{
    return DeviceList.GetBrokenDevices();
}

TVector<NProto::TDeviceConfig> TDiskRegistryState::GetDirtyDevices() const
{
    return DeviceList.GetDirtyDevices();
}

bool TDiskRegistryState::IsKnownDevice(const TString& uuid) const
{
    return std::any_of(
        KnownAgents.begin(),
        KnownAgents.end(),
        [&] (const auto& kv) {
            return kv.second.Devices.contains(uuid);
        });
}

NProto::TError TDiskRegistryState::MarkDiskForCleanup(
    TDiskRegistryDatabase& db,
    const TString& diskId)
{
    if (!Disks.contains(diskId)) {
        return MakeError(E_NOT_FOUND, TStringBuilder() << "disk " <<
            diskId.Quote() << " not found");
    }

    db.AddDiskToCleanup(diskId);
    DisksToCleanup.insert(diskId);

    return {};
}

bool TDiskRegistryState::MarkDeviceAsDirty(
    TDiskRegistryDatabase& db,
    const TDeviceId& uuid)
{
    if (!IsKnownDevice(uuid)) {
        return false;
    }

    DeviceList.MarkDeviceAsDirty(uuid);
    db.UpdateDirtyDevice(uuid, {});

    return true;
}

TString TDiskRegistryState::MarkDeviceAsClean(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TDeviceId& uuid)
{
    DeviceList.MarkDeviceAsClean(uuid);
    db.DeleteDirtyDevice(uuid);

    if (!DeviceList.IsSuspendedDevice(uuid)) {
        db.DeleteSuspendedDevice(uuid);
    }

    TryUpdateDevice(now, db, uuid);

    return PendingCleanup.EraseDevice(uuid);
}

bool TDiskRegistryState::TryUpdateDevice(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TDeviceId& uuid)
{
    Y_UNUSED(now);

    auto [agent, device] = FindDeviceLocation(uuid);
    if (!agent || !device) {
        return false;
    }

    AdjustDeviceIfNeeded(*device, {});

    UpdateAgent(db, *agent);
    DeviceList.UpdateDevices(*agent);

    return true;
}

TVector<TString> TDiskRegistryState::CollectBrokenDevices(
    const NProto::TAgentStats& stats) const
{
    TVector<TString> uuids;

    for (const auto& ds: stats.GetDeviceStats()) {
        if (!ds.GetErrors()) {
            continue;
        }

        const auto& uuid = ds.GetDeviceUUID();

        const auto* device = DeviceList.FindDevice(uuid);
        if (!device) {
            continue;
        }

        Y_VERIFY_DEBUG(device->GetNodeId() == stats.GetNodeId());

        if (device->GetState() != NProto::DEVICE_STATE_ERROR) {
            uuids.push_back(uuid);
        }
    }

    return uuids;
}

NProto::TError TDiskRegistryState::UpdateAgentCounters(
    const NProto::TAgentStats& stats)
{
    // TODO (NBS-3280): add AgentId to TAgentStats
    const auto* agent = AgentList.FindAgent(stats.GetNodeId());

    if (!agent) {
        return MakeError(E_NOT_FOUND, "agent not found");
    }

    for (const auto& device: stats.GetDeviceStats()) {
        const auto& uuid = device.GetDeviceUUID();

        if (stats.GetNodeId() != DeviceList.FindNodeId(uuid)) {
            return MakeError(E_ARGUMENT, "unexpected device");
        }
    }

    AgentList.UpdateCounters(stats, agent->GetTimeBetweenFailures());

    return {};
}

THashMap<TString, TBrokenGroupInfo> TDiskRegistryState::GatherBrokenGroupsInfo(
    TInstant now,
    TDuration period) const
{
    THashMap<TString, TBrokenGroupInfo> groups;

    for (const auto& x: Disks) {
        if (x.second.State != NProto::DISK_STATE_TEMPORARILY_UNAVAILABLE &&
            x.second.State != NProto::DISK_STATE_ERROR)
        {
            continue;
        }

        if (x.second.PlacementGroupId.empty()) {
            continue;
        }

        TBrokenGroupInfo& info = groups[x.second.PlacementGroupId];

        ++info.TotalBrokenDiskCount;
        if (now - period < x.second.StateTs) {
            ++info.RecentlyBrokenDiskCount;
        }
    }

    return groups;
}

void TDiskRegistryState::PublishCounters(TInstant now)
{
    if (!Counters) {
        return;
    }

    AgentList.PublishCounters(now);

    THashMap<TString, TDevicePoolCounters> poolName2Counters;

    ui32 allocatedDisks = 0;
    ui32 agentsInOnlineState = 0;
    ui32 agentsInWarningState = 0;
    ui32 agentsInUnavailableState = 0;
    ui32 disksInOnlineState = 0;
    ui32 disksInMigrationState = 0;
    ui32 disksInTemporarilyUnavailableState = 0;
    ui32 disksInErrorState = 0;
    ui32 placementGroups = 0;
    ui32 fullPlacementGroups = 0;
    ui32 allocatedDisksInGroups = 0;

    for (const auto& agent: AgentList.GetAgents()) {
        switch (agent.GetState()) {
            case NProto::AGENT_STATE_ONLINE: {
                ++agentsInOnlineState;
                break;
            }
            case NProto::AGENT_STATE_WARNING: {
                ++agentsInWarningState;
                break;
            }
            case NProto::AGENT_STATE_UNAVAILABLE: {
                ++agentsInUnavailableState;
                break;
            }
            default: {}
        }

        for (const auto& device: agent.GetDevices()) {
            const auto deviceState = device.GetState();
            const auto deviceBytes = device.GetBlockSize() * device.GetBlocksCount();
            const bool allocated = !DeviceList.FindDiskId(device.GetDeviceUUID()).empty();
            const bool dirty = DeviceList.IsDirtyDevice(device.GetDeviceUUID());

            const auto poolName = GetPoolNameForCounters(
                device.GetPoolName(),
                device.GetPoolKind());
            auto& pool = poolName2Counters[poolName];

            pool.TotalBytes += deviceBytes;

            if (allocated) {
                ++pool.AllocatedDevices;
            }

            if (dirty) {
                ++pool.DirtyDevices;
            }

            switch (deviceState) {
                case NProto::DEVICE_STATE_ONLINE: {
                    if (!allocated && !dirty && agent.GetState() == NProto::AGENT_STATE_ONLINE) {
                        pool.FreeBytes += deviceBytes;
                    }

                    ++pool.DevicesInOnlineState;
                    break;
                }
                case NProto::DEVICE_STATE_WARNING: {
                    ++pool.DevicesInWarningState;
                    break;
                }
                case NProto::DEVICE_STATE_ERROR: {
                    ++pool.DevicesInErrorState;
                    break;
                }
                default: {}
            }
        }
    }

    placementGroups = PlacementGroups.size();

    for (auto& [_, pg]: PlacementGroups) {
        allocatedDisksInGroups += pg.Config.DisksSize();

        const auto limit =
            GetMaxDisksInPlacementGroup(*StorageConfig, pg.Config);
        if (pg.Config.DisksSize() >= limit || pg.Config.DisksSize() == 0) {
            continue;
        }

        pg.BiggestDiskId = {};
        pg.BiggestDiskSize = 0;
        pg.Full = false;
        ui32 logicalBlockSize = 0;
        for (const auto& diskInfo: pg.Config.GetDisks()) {
            auto* disk = Disks.FindPtr(diskInfo.GetDiskId());

            if (!disk) {
                ReportDiskRegistryDiskNotFound(
                    TStringBuilder() << "PublishCounters:DiskId: "
                    << diskInfo.GetDiskId());

                continue;
            }

            ui64 diskSize = 0;
            for (const auto& deviceId: disk->Devices) {
                const auto& device = DeviceList.FindDevice(deviceId);

                if (!device) {
                    ReportDiskRegistryDeviceNotFound(
                        TStringBuilder() << "PublishCounters:DiskId: "
                        << diskInfo.GetDiskId()
                        << ", DeviceId: " << deviceId);

                    continue;
                }

                diskSize += device->GetBlockSize() * device->GetBlocksCount();
            }

            if (diskSize > pg.BiggestDiskSize) {
                logicalBlockSize = disk->LogicalBlockSize;
                pg.BiggestDiskId = diskInfo.GetDiskId();
                pg.BiggestDiskSize = diskSize;
            }
        }

        if (!logicalBlockSize) {
            continue;
        }

        THashSet<TString> forbiddenRacks;
        CollectForbiddenRacks(pg.Config, &forbiddenRacks);

        for (const auto& [_, pool]: DevicePoolConfigs) {
            if (pool.GetKind() == NProto::DEVICE_POOL_KIND_LOCAL) {
                continue;
            }

            const TDeviceList::TAllocationQuery query {
                .ForbiddenRacks = forbiddenRacks,
                .LogicalBlockSize = logicalBlockSize,
                .BlockCount = pg.BiggestDiskSize / logicalBlockSize,
                .PoolName = pool.GetName(),
                .PoolKind = pool.GetKind(),
                .NodeIds = {}
            };

            if (!DeviceList.CanAllocateDevices(query)) {
                ++fullPlacementGroups;
                pg.Full = true;
                break;
            }
        }
    }

    allocatedDisks = Disks.size();

    TDuration maxMigrationTime;

    for (const auto& [_, disk]: Disks) {
        switch (disk.State) {
            case NProto::DISK_STATE_ONLINE: {
                ++disksInOnlineState;
                break;
            }
            case NProto::DISK_STATE_MIGRATION: {
                ++disksInMigrationState;

                maxMigrationTime = std::max(maxMigrationTime, now - disk.StateTs);

                break;
            }
            case NProto::DISK_STATE_TEMPORARILY_UNAVAILABLE: {
                ++disksInTemporarilyUnavailableState;
                break;
            }
            case NProto::DISK_STATE_ERROR: {
                ++disksInErrorState;
                break;
            }
            default: {}
        }
    }

    ui64 freeBytes = 0;
    ui64 totalBytes = 0;
    ui64 allocatedDevices = 0;
    ui64 dirtyDevices = 0;
    ui64 devicesInOnlineState = 0;
    ui64 devicesInWarningState = 0;
    ui64 devicesInErrorState = 0;
    for (const auto& [poolName, counterValues]: poolName2Counters) {
        if (auto* pc = SelfCounters.PoolName2Counters.FindPtr(poolName)) {
            SetDevicePoolCounters(*pc, counterValues);
        }

        freeBytes += counterValues.FreeBytes;
        totalBytes += counterValues.TotalBytes;
        allocatedDevices += counterValues.AllocatedDevices;
        dirtyDevices += counterValues.DirtyDevices;
        devicesInOnlineState += counterValues.DevicesInOnlineState;
        devicesInWarningState += counterValues.DevicesInWarningState;
        devicesInErrorState += counterValues.DevicesInErrorState;
    }

    SelfCounters.FreeBytes->Set(freeBytes);
    SelfCounters.TotalBytes->Set(totalBytes);
    SelfCounters.AllocatedDevices->Set(allocatedDevices);
    SelfCounters.DirtyDevices->Set(dirtyDevices);
    SelfCounters.DevicesInOnlineState->Set(devicesInOnlineState);
    SelfCounters.DevicesInWarningState->Set(devicesInWarningState);
    SelfCounters.DevicesInErrorState->Set(devicesInErrorState);

    SelfCounters.AllocatedDisks->Set(allocatedDisks);
    SelfCounters.AgentsInOnlineState->Set(agentsInOnlineState);
    SelfCounters.AgentsInWarningState->Set(agentsInWarningState);
    SelfCounters.AgentsInUnavailableState->Set(agentsInUnavailableState);
    SelfCounters.DisksInOnlineState->Set(disksInOnlineState);
    SelfCounters.DisksInMigrationState->Set(disksInMigrationState);
    SelfCounters.DevicesInMigrationState->Set(
        DeviceMigrationsInProgress + Migrations.size());
    SelfCounters.DisksInTemporarilyUnavailableState->Set(
        disksInTemporarilyUnavailableState);
    SelfCounters.DisksInErrorState->Set(disksInErrorState);
    SelfCounters.PlacementGroups->Set(placementGroups);
    SelfCounters.FullPlacementGroups->Set(fullPlacementGroups);
    SelfCounters.AllocatedDisksInGroups->Set(allocatedDisksInGroups);
    SelfCounters.MaxMigrationTime->Set(maxMigrationTime.Seconds());

    SelfCounters.MeanTimeBetweenFailures->Set(
        TimeBetweenFailures.GetBrokenCount()
        ? TimeBetweenFailures.GetWorkTime() /
            TimeBetweenFailures.GetBrokenCount()
        : 0);

    ui32 placementGroupsWithRecentlyBrokenSingleDisk = 0;
    ui32 placementGroupsWithRecentlyBrokenTwoOrMoreDisks = 0;
    ui32 placementGroupsWithBrokenSingleDisk = 0;
    ui32 placementGroupsWithBrokenTwoOrMoreDisks = 0;

    auto brokenGroups = GatherBrokenGroupsInfo(
        now,
        StorageConfig->GetPlacementGroupAlertPeriod());

    for (const auto& kv: brokenGroups) {
        const auto [total, recently] = kv.second;

        placementGroupsWithRecentlyBrokenSingleDisk += recently == 1;
        placementGroupsWithRecentlyBrokenTwoOrMoreDisks += recently > 1;
        placementGroupsWithBrokenSingleDisk += total == 1;
        placementGroupsWithBrokenTwoOrMoreDisks += total > 1;
    }

    SelfCounters.PlacementGroupsWithRecentlyBrokenSingleDisk->Set(
        placementGroupsWithRecentlyBrokenSingleDisk);

    SelfCounters.PlacementGroupsWithRecentlyBrokenTwoOrMoreDisks->Set(
        placementGroupsWithRecentlyBrokenTwoOrMoreDisks);

    SelfCounters.PlacementGroupsWithBrokenSingleDisk->Set(
        placementGroupsWithBrokenSingleDisk);

    SelfCounters.PlacementGroupsWithBrokenTwoOrMoreDisks->Set(
        placementGroupsWithBrokenTwoOrMoreDisks);

    auto replicaCountStats = ReplicaTable.CalculateReplicaCountStats();
    SelfCounters.Mirror2Disks->Set(replicaCountStats.Mirror2DiskMinus0);
    SelfCounters.Mirror2DisksMinus1->Set(replicaCountStats.Mirror2DiskMinus1);
    SelfCounters.Mirror2DisksMinus2->Set(replicaCountStats.Mirror2DiskMinus2);
    SelfCounters.Mirror3Disks->Set(replicaCountStats.Mirror3DiskMinus0);
    SelfCounters.Mirror3DisksMinus1->Set(replicaCountStats.Mirror3DiskMinus1);
    SelfCounters.Mirror3DisksMinus2->Set(replicaCountStats.Mirror3DiskMinus2);
    SelfCounters.Mirror3DisksMinus3->Set(replicaCountStats.Mirror3DiskMinus3);
    SelfCounters.MaxMigrationTime->Set(maxMigrationTime.Seconds());

    SelfCounters.AutomaticallyReplacedDevices->Set(
        AutomaticallyReplacedDevices.size());

    SelfCounters.QueryAvailableStorageErrors.Publish(now);
    SelfCounters.QueryAvailableStorageErrors.Reset();
}

NProto::TError TDiskRegistryState::CreatePlacementGroup(
    TDiskRegistryDatabase& db,
    const TString& groupId,
    NProto::EPlacementStrategy placementStrategy,
    ui32 placementPartitionCount)
{
    if (PlacementGroups.contains(groupId)) {
        return MakeError(S_ALREADY);
    }

    if (placementStrategy ==
        NProto::EPlacementStrategy::PLACEMENT_STRATEGY_SPREAD)
    {
        if (placementPartitionCount) {
            return MakeError(
                E_ARGUMENT,
                "Partition count for spread placement group shouldn't be "
                "specified");
        }
    }

    if (placementStrategy ==
        NProto::EPlacementStrategy::PLACEMENT_STRATEGY_PARTITION)
    {
        if (placementPartitionCount < 2 ||
            placementPartitionCount > StorageConfig->GetMaxPlacementPartitionCount())
        {
            return MakeError(E_ARGUMENT, TStringBuilder()
                << "Partitions count must be between 2 and "
                << StorageConfig->GetMaxPlacementPartitionCount());
        }
    }

    auto& g = PlacementGroups[groupId].Config;
    g.SetGroupId(groupId);
    g.SetConfigVersion(1);
    g.SetPlacementStrategy(placementStrategy);
    g.SetPlacementPartitionCount(placementPartitionCount);
    db.UpdatePlacementGroup(g);

    return {};
}

NProto::TError TDiskRegistryState::CheckPlacementGroupVersion(
    const TString& placementGroupId,
    ui32 configVersion)
{
    auto* config = FindPlacementGroup(placementGroupId);
    if (!config) {
        return MakeError(E_NOT_FOUND, TStringBuilder()
            << "group does not exist: " << placementGroupId);
    }

    if (config->GetConfigVersion() != configVersion) {
        return MakeError(E_ABORTED, TStringBuilder()
                << "received version != expected version: "
                << configVersion << " != " << config->GetConfigVersion());
    }

    return {};
}

NProto::TError TDiskRegistryState::UpdatePlacementGroupSettings(
    TDiskRegistryDatabase& db,
    const TString& groupId,
    ui32 configVersion,
    NProto::TPlacementGroupSettings settings)
{
    auto error = CheckPlacementGroupVersion(groupId, configVersion);
    if (HasError(error)) {
        return error;
    }

    auto& config = PlacementGroups[groupId].Config;
    config.SetConfigVersion(configVersion + 1);
    config.MutableSettings()->SetMaxDisksInGroup(settings.GetMaxDisksInGroup());
    db.UpdatePlacementGroup(config);

    return {};
}

NProto::TError TDiskRegistryState::DestroyPlacementGroup(
    TDiskRegistryDatabase& db,
    const TString& groupId,
    TVector<TString>& affectedDisks)
{
    auto* g = FindPlacementGroup(groupId);
    if (!g) {
        return MakeError(S_ALREADY);
    }

    for (const auto& diskInfo: g->GetDisks()) {
        auto& diskId = diskInfo.GetDiskId();
        auto* d = Disks.FindPtr(diskId);
        if (!d) {
            ReportDiskRegistryDiskNotFound(TStringBuilder()
                << "DestroyPlacementGroup:DiskId: " << diskId
                << ", PlacementGroupId: " << groupId);

            continue;
        }

        d->PlacementGroupId.clear();

        NotificationSystem.AddOutdatedVolumeConfig(db, diskId);

        affectedDisks.emplace_back(diskId);
    }

    db.DeletePlacementGroup(g->GetGroupId());
    PlacementGroups.erase(g->GetGroupId());

    return {};
}

NProto::TError TDiskRegistryState::AlterPlacementGroupMembership(
    TDiskRegistryDatabase& db,
    const TString& groupId,
    ui32 placementPartitionIndex,
    ui32 configVersion,
    TVector<TString>& disksToAdd,
    const TVector<TString>& disksToRemove)
{
    if (auto error = CheckPlacementGroupVersion(groupId, configVersion); HasError(error)) {
        return error;
    }

    if (disksToAdd) {
        auto error = CheckDiskPlacementInfo({groupId, placementPartitionIndex});
        if (HasError(error)) {
            return error;
        }
    }

    auto newG = PlacementGroups[groupId].Config;

    if (disksToRemove) {
        auto end = std::remove_if(
            newG.MutableDisks()->begin(),
            newG.MutableDisks()->end(),
            [&] (const NProto::TPlacementGroupConfig::TDiskInfo& d) {
                return Find(disksToRemove.begin(), disksToRemove.end(), d.GetDiskId())
                    != disksToRemove.end();
            }
        );

        while (newG.MutableDisks()->end() > end) {
            newG.MutableDisks()->RemoveLast();
        }
    }

    TVector<TString> failedToAdd;
    THashMap<TString, TSet<TString>> disk2racks;
    for (const auto& diskId: disksToAdd) {
        const bool diskInCorrectLocation =
            FindIfPtr(
                newG.GetDisks(),
                [&] (const NProto::TPlacementGroupConfig::TDiskInfo& d) {
                    return (d.GetDiskId() == diskId) &&
                           (!PlacementGroupMustHavePartitions(newG) ||
                            d.GetPlacementPartitionIndex() == placementPartitionIndex);
                }
            ) != nullptr;

        if (diskInCorrectLocation) {
            continue;
        }

        const auto* disk = Disks.FindPtr(diskId);
        if (!disk) {
            return MakeError(
                E_ARGUMENT,
                TStringBuilder() << "no such nonreplicated disk: " << diskId
                    << " - wrong media kind specified during disk creation?"
            );
        }

        if (disk->PlacementGroupId) {
            failedToAdd.push_back(diskId);
            continue;
        }
        THashSet<TString> forbiddenRacks;
        THashSet<TString> preferredRacks;
        CollectRacks(
            diskId,
            placementPartitionIndex,
            newG,
            &forbiddenRacks,
            &preferredRacks);

        // consider the racks of checked disks to be forbidden for next disks
        if (newG.GetPlacementStrategy() == NProto::PLACEMENT_STRATEGY_SPREAD) {
            for (const auto& [disk, racks]: disk2racks) {
                for (const auto& rack: racks) {
                    forbiddenRacks.insert(rack);
                }
            }
        }

        bool canAdd = true;

        for (const auto& deviceUuid: disk->Devices) {
            const auto rack = DeviceList.FindRack(deviceUuid);
            if (forbiddenRacks.contains(rack)) {
                canAdd = false;
                break;
            }
        }

        if (!canAdd) {
            failedToAdd.push_back(diskId);
            continue;
        }

        auto& racks = disk2racks[diskId];
        for (const auto& deviceUuid: disk->Devices) {
            racks.insert(DeviceList.FindRack(deviceUuid));
        }
    }

    if (failedToAdd.size()) {
        disksToAdd = std::move(failedToAdd);
        return MakeError(E_PRECONDITION_FAILED, "failed to add some disks");
    }

    if (newG.DisksSize() + disk2racks.size()
            > GetMaxDisksInPlacementGroup(*StorageConfig, newG))
    {
        ui32 flags = 0;
        SetProtoFlag(flags, NProto::EF_SILENT);

        return MakeError(
            E_BS_RESOURCE_EXHAUSTED,
            TStringBuilder()
                << "max disk count in group exceeded, max: "
                << GetMaxDisksInPlacementGroup(*StorageConfig, newG),
            flags);
    }

    for (const auto& [diskId, racks]: disk2racks) {
        auto& d = *newG.AddDisks();
        d.SetDiskId(diskId);
        d.SetPlacementPartitionIndex(placementPartitionIndex);
        for (const auto& rack: racks) {
            *d.AddDeviceRacks() = rack;
        }
    }
    newG.SetConfigVersion(configVersion + 1);
    db.UpdatePlacementGroup(newG);
    PlacementGroups[groupId] = std::move(newG);

    for (const auto& diskId: disksToAdd) {
        auto* d = Disks.FindPtr(diskId);

        if (!d) {
            ReportDiskRegistryDiskNotFound(TStringBuilder()
                << "AlterPlacementGroupMembership:DiskId: " << diskId
                << ", PlacementGroupId: " << groupId
                << ", PlacementPartitionIndex: " << placementPartitionIndex);

            continue;
        }

        d->PlacementGroupId = groupId;
        d->PlacementPartitionIndex = placementPartitionIndex;

        NotificationSystem.AddOutdatedVolumeConfig(db, diskId);
    }

    for (const auto& diskId: disksToRemove) {
        if (FindPtr(disksToAdd, diskId)) {
            continue;
        }

        if (auto* d = Disks.FindPtr(diskId)) {
            d->PlacementGroupId.clear();
            d->PlacementPartitionIndex = 0;

            NotificationSystem.AddOutdatedVolumeConfig(db, diskId);
        }
    }

    disksToAdd.clear();

    return {};
}

const NProto::TPlacementGroupConfig* TDiskRegistryState::FindPlacementGroup(
    const TString& groupId) const
{
    if (auto g = PlacementGroups.FindPtr(groupId)) {
        return &g->Config;
    }

    return nullptr;
}

void TDiskRegistryState::DeleteBrokenDisks(
    TDiskRegistryDatabase& db,
    TVector<TDiskId> ids)
{
    for (const auto& id: ids) {
        db.DeleteBrokenDisk(id);
    }

    Sort(ids);
    SortBy(BrokenDisks, [] (const auto& d) {
        return d.DiskId;
    });

    TVector<TBrokenDiskInfo> newList;

    std::set_difference(
        BrokenDisks.begin(),
        BrokenDisks.end(),
        ids.begin(),
        ids.end(),
        std::back_inserter(newList),
        TOverloaded {
            [] (const TBrokenDiskInfo& lhs, const auto& rhs) {
                return lhs.DiskId < rhs;
            },
            [] (const auto& lhs, const auto& rhs) {
                return lhs < rhs.DiskId;
            }});

    BrokenDisks.swap(newList);
}

void TDiskRegistryState::UpdateAndReallocateDisk(
    TDiskRegistryDatabase& db,
    const TString& diskId,
    TDiskState& disk)
{
    db.UpdateDisk(BuildDiskConfig(diskId, disk));
    AddReallocateRequest(db, diskId);
}

ui64 TDiskRegistryState::AddReallocateRequest(
    TDiskRegistryDatabase& db,
    TString diskId)
{
    const auto* disk = Disks.FindPtr(diskId);
    Y_VERIFY_DEBUG(disk, "unknown disk: %s", diskId.c_str());

    if (disk && disk->MasterDiskId) {
        diskId = disk->MasterDiskId;
    }

    return NotificationSystem.AddReallocateRequest(db, diskId);
}

const THashMap<TString, ui64>& TDiskRegistryState::GetDisksToReallocate() const
{
    return NotificationSystem.GetDisksToReallocate();
}

auto TDiskRegistryState::FindDiskState(const TDiskId& diskId) -> TDiskState*
{
    auto it = Disks.find(diskId);
    if (it == Disks.end()) {
        return nullptr;
    }
    return &it->second;
}

void TDiskRegistryState::RemoveFinishedMigrations(
    TDiskRegistryDatabase& db,
    const TString& diskId,
    ui64 seqNo)
{
    auto* disk = FindDiskState(diskId);
    if (!disk) {
        return;
    }

    if (disk->ReplicaCount) {
        for (ui32 i = 0; i < disk->ReplicaCount + 1; ++i) {
            RemoveFinishedMigrations(
                db,
                GetReplicaDiskId(diskId, i),
                seqNo);
        }

        return;
    }

    auto& migrations = disk->FinishedMigrations;

    auto it = std::remove_if(
        migrations.begin(),
        migrations.end(),
        [&] (const auto& m) {
            if (m.SeqNo > seqNo) {
                return false;
            }

            DeviceList.ReleaseDevice(m.DeviceId);
            db.UpdateDirtyDevice(m.DeviceId, diskId);
            PendingCleanup.Insert(diskId, m.DeviceId);

            return true;
        }
    );

    if (it != migrations.end()) {
        migrations.erase(it, migrations.end());
        db.UpdateDisk(BuildDiskConfig(diskId, *disk));
    }
}

void TDiskRegistryState::DeleteDiskToReallocate(
    TDiskRegistryDatabase& db,
    const TString& diskId,
    ui64 seqNo)
{
    NotificationSystem.DeleteDiskToReallocate(db, diskId, seqNo);
    RemoveFinishedMigrations(db, diskId, seqNo);
}

void TDiskRegistryState::AddUserNotification(
    TDiskRegistryDatabase& db,
    NProto::TUserNotification notification)
{
    NotificationSystem.AddUserNotification(db, std::move(notification));
}

void TDiskRegistryState::DeleteUserNotification(
    TDiskRegistryDatabase& db,
    const TString& entityId,
    ui64 seqNo)
{
    NotificationSystem.DeleteUserNotification(db, entityId, seqNo);
}

void TDiskRegistryState::GetUserNotifications(
    TVector<NProto::TUserNotification>& notifications) const
{
    return NotificationSystem.GetUserNotifications(notifications);
}

NProto::TDiskConfig TDiskRegistryState::BuildDiskConfig(
    TDiskId diskId,
    const TDiskState& diskState) const
{
    NProto::TDiskConfig config;

    config.SetDiskId(std::move(diskId));
    config.SetBlockSize(diskState.LogicalBlockSize);
    config.SetState(diskState.State);
    config.SetStateTs(diskState.StateTs.MicroSeconds());
    config.SetCloudId(diskState.CloudId);
    config.SetFolderId(diskState.FolderId);
    config.SetUserId(diskState.UserId);
    config.SetReplicaCount(diskState.ReplicaCount);
    config.SetMasterDiskId(diskState.MasterDiskId);

    for (const auto& [uuid, seqNo]: diskState.FinishedMigrations) {
        Y_UNUSED(seqNo);
        auto& m = *config.AddFinishedMigrations();
        m.SetDeviceId(uuid);
    }

    for (const auto& id: diskState.DeviceReplacementIds) {
        *config.AddDeviceReplacementUUIDs() = id;
    }

    for (const auto& uuid: diskState.Devices) {
        *config.AddDeviceUUIDs() = uuid;
    }

    for (const auto& [targetId, sourceId]: diskState.MigrationTarget2Source) {
        auto& m = *config.AddMigrations();
        m.SetSourceDeviceId(sourceId);
        m.MutableTargetDevice()->SetDeviceUUID(targetId);
    }

    return config;
}

void TDiskRegistryState::DeleteDiskStateChanges(
    TDiskRegistryDatabase& db,
    const TString& diskId,
    ui64 seqNo)
{
    db.DeleteDiskStateChanges(diskId, seqNo);
}

NProto::TError TDiskRegistryState::CheckAgentStateTransition(
    const TString& agentId,
    NProto::EAgentState newState,
    TInstant timestamp) const
{
    const auto* agent = AgentList.FindAgent(agentId);

    if (!agent) {
        return MakeError(E_NOT_FOUND, "agent not found");
    }

    if (agent->GetState() == newState) {
        return MakeError(S_ALREADY);
    }

    if (agent->GetStateTs() > timestamp.MicroSeconds()) {
        return MakeError(E_INVALID_STATE, "out of order");
    }

    return {};
}

NProto::TError TDiskRegistryState::UpdateAgentState(
    TDiskRegistryDatabase& db,
    TString agentId,
    NProto::EAgentState newState,
    TInstant timestamp,
    TString reason,
    TVector<TDiskId>& affectedDisks)
{
    auto error = CheckAgentStateTransition(agentId, newState, timestamp);
    if (FAILED(error.GetCode())) {
        return error;
    }

    auto* agent = AgentList.FindAgent(agentId);
    if (!agent) {
        auto message = ReportDiskRegistryAgentNotFound(
            TStringBuilder() << "UpdateAgentState:AgentId: " << agentId);

        return MakeError(E_FAIL, agentId);
    }

    const auto cmsTs = TInstant::MicroSeconds(agent->GetCmsTs());
    const auto oldState = agent->GetState();
    const auto cmsDeadline = cmsTs + GetInfraTimeout(*StorageConfig, oldState);
    const auto cmsRequestActive = cmsTs && cmsDeadline > timestamp;

    if (!cmsRequestActive) {
        agent->SetCmsTs(0);
    }

    // when newState is less than AGENT_STATE_WARNING
    // check if agent is not scheduled for shutdown by cms
    if ((newState < NProto::EAgentState::AGENT_STATE_WARNING) && cmsRequestActive) {
        newState = oldState;
    }

    ChangeAgentState(*agent, newState, timestamp, std::move(reason));

    ApplyAgentStateChange(db, *agent, timestamp, affectedDisks);

    return error;
}

void TDiskRegistryState::ApplyAgentStateChange(
    TDiskRegistryDatabase& db,
    const NProto::TAgentConfig& agent,
    TInstant timestamp,
    TVector<TDiskId>& affectedDisks)
{
    UpdateAgent(db, agent);
    DeviceList.UpdateDevices(agent);

    THashSet<TString> diskIds;

    for (const auto& d: agent.GetDevices()) {
        const auto& deviceId = d.GetDeviceUUID();
        auto diskId = DeviceList.FindDiskId(deviceId);

        if (diskId.empty()) {
            continue;
        }

        auto& disk = Disks[diskId];

        // check if deviceId is target for migration
        if (RestartDeviceMigration(db, diskId, disk, deviceId)) {
            continue;
        }

        bool isAffected = true;

        if (agent.GetState() == NProto::AGENT_STATE_WARNING) {
            if (disk.MigrationSource2Target.contains(deviceId)) {
                // migration already started
                continue;
            }

            bool found = false;
            for (const auto& d: disk.Devices) {
                if (d == deviceId) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                // this device is allocated for this disk but it is not among
                // this disk's active device set
                // NBS-3726
                continue;
            }

            AddMigration(disk, diskId, deviceId);
        } else {
            if (agent.GetState() == NProto::AGENT_STATE_UNAVAILABLE
                    && disk.MasterDiskId)
            {
                const bool canReplaceDevice = ReplicaTable.IsReplacementAllowed(
                    disk.MasterDiskId,
                    deviceId);

                if (canReplaceDevice) {
                    bool updated = false;

                    auto error = ReplaceDevice(
                        db,
                        diskId,
                        deviceId,
                        "",     // no replacement device
                        timestamp,
                        MakeMirroredDiskDeviceReplacementMessage(
                            disk.MasterDiskId,
                            "agent unavailable"),
                        false,  // manual
                        &updated);

                    if (HasError(error)) {
                        ReportMirroredDiskDeviceReplacementFailure(
                            FormatError(error));
                    }

                    if (!updated) {
                        isAffected = false;
                    }
                } else {
                    ReportMirroredDiskDeviceReplacementForbidden();
                }
            }

            CancelDeviceMigration(db, diskId, disk, deviceId);
        }

        if (isAffected) {
            diskIds.emplace(std::move(diskId));
        }
    }

    for (auto& id: diskIds) {
        if (TryUpdateDiskState(db, id, timestamp)) {
            affectedDisks.push_back(std::move(id));
        }
    }
}

bool TDiskRegistryState::HasDependentDisks(const NProto::TAgentConfig& agent) const
{
    for (const auto& d: agent.GetDevices()) {
        if (d.GetState() >= NProto::DEVICE_STATE_ERROR) {
            continue;
        }

        const auto diskId = FindDisk(d.GetDeviceUUID());

        if (!diskId) {
            continue;
        }

        const auto* disk = Disks.FindPtr(diskId);
        if (!disk) {
            ReportDiskRegistryDiskNotFound(
                TStringBuilder() << "HasDependentDisks:DiskId: " << diskId);
            continue;
        }

        return true;
    }

    return false;
}

NProto::TError TDiskRegistryState::UpdateCmsHostState(
    TDiskRegistryDatabase& db,
    TString agentId,
    NProto::EAgentState newState,
    TInstant now,
    bool dryRun,
    TVector<TDiskId>& affectedDisks,
    TDuration& timeout)
{
    auto error = CheckAgentStateTransition(agentId, newState, now);
    if (FAILED(error.GetCode())) {
        return error;
    }

    auto* agent = AgentList.FindAgent(agentId);
    if (!agent) {
        auto message = ReportDiskRegistryAgentNotFound(
            TStringBuilder() << "UpdateCmsHostState:AgentId: " << agentId);

        return MakeError(E_FAIL, agentId);
    }

    TInstant cmsTs = TInstant::MicroSeconds(agent->GetCmsTs());
    if (cmsTs == TInstant::Zero()) {
        cmsTs = now;
    }

    const auto infraTimeout = GetInfraTimeout(*StorageConfig, agent->GetState());

    if (cmsTs + infraTimeout <= now
            && agent->GetState() < NProto::AGENT_STATE_UNAVAILABLE)
    {
        // restart timer
        cmsTs = now;
    }

    timeout = cmsTs + infraTimeout - now;

    const bool hasDependentDisks = HasDependentDisks(*agent);
    if (!hasDependentDisks) {
        // no dependent disks => we can return this host immediately
        timeout = TDuration::Zero();
    }

    if (newState == NProto::AGENT_STATE_ONLINE
        && agent->GetState() < NProto::AGENT_STATE_UNAVAILABLE)
    {
        timeout = TDuration::Zero();
    }

    NProto::TError result;
    if (timeout == TDuration::Zero()) {
        result = MakeError(S_OK);
        cmsTs = TInstant::Zero();
    } else {
        result = MakeError(
            E_TRY_AGAIN,
            TStringBuilder() << "time remaining: " << timeout);
    }

    if (agent->GetState() == NProto::AGENT_STATE_UNAVAILABLE) {
        // Agent can return from 'unavailable' state only when it is reconnected
        // to the cluster.
        if (newState == NProto::AGENT_STATE_ONLINE) {
            // Should retry while agent is in 'unavailable' state.
            result = MakeError(E_TRY_AGAIN, "agent currently unavailable");
            if (timeout == TDuration::Zero()) {
                timeout = CMS_UPDATE_STATE_TO_ONLINE_TIMEOUT;
            }
        }
    }

    if (dryRun) {
        return result;
    }

    if (agent->GetState() != NProto::AGENT_STATE_UNAVAILABLE) {
        ChangeAgentState(*agent, newState, now, "cms action");
    }

    agent->SetCmsTs(cmsTs.MicroSeconds());

    ApplyAgentStateChange(db, *agent, now, affectedDisks);

    if (newState != NProto::AGENT_STATE_ONLINE) {
        SuspendLocalDevices(db, *agent);
    }

    return result;
}

void TDiskRegistryState::SuspendLocalDevices(
    TDiskRegistryDatabase& db,
    const NProto::TAgentConfig& agent)
{
    for (const auto& d: agent.GetDevices()) {
        if (d.GetPoolKind() == NProto::DEVICE_POOL_KIND_LOCAL) {
            SuspendDevice(db, d.GetDeviceUUID());
        }
    }
}

TMaybe<NProto::EAgentState> TDiskRegistryState::GetAgentState(
    const TString& agentId) const
{
    const auto* agent = AgentList.FindAgent(agentId);
    if (agent) {
        return agent->GetState();
    }

    return {};
}

TMaybe<TInstant> TDiskRegistryState::GetAgentCmsTs(
    const TString& agentId) const
{
    const auto* agent = AgentList.FindAgent(agentId);
    if (agent) {
        return TInstant::MicroSeconds(agent->GetCmsTs());
    }

    return {};
}

bool TDiskRegistryState::TryUpdateDiskState(
    TDiskRegistryDatabase& db,
    const TString& diskId,
    TInstant timestamp)
{
    auto* d = Disks.FindPtr(diskId);

    if (!d) {
        ReportDiskRegistryDiskNotFound(TStringBuilder()
            << "TryUpdateDiskState:DiskId: " << diskId);

        return {};
    }

    return TryUpdateDiskState(
        db,
        diskId,
        *d,
        timestamp);
}

bool TDiskRegistryState::TryUpdateDiskState(
    TDiskRegistryDatabase& db,
    const TString& diskId,
    TDiskState& disk,
    TInstant timestamp)
{
    const auto newState = CalculateDiskState(disk);
    const auto oldState = disk.State;
    if (oldState == newState) {
        return false;
    }

    disk.State = newState;
    disk.StateTs = timestamp;

    UpdateAndReallocateDisk(db, diskId, disk);

    NotificationSystem.OnDiskStateChanged(
        db,
        diskId,
        oldState,
        newState,
        timestamp);

    return true;
}

void TDiskRegistryState::DeleteDiskStateUpdate(
    TDiskRegistryDatabase& db,
    ui64 maxSeqNo)
{
    NotificationSystem.DeleteDiskStateUpdate(db, maxSeqNo);
}

NProto::EDiskState TDiskRegistryState::CalculateDiskState(
    const TDiskState& disk) const
{
    NProto::EDiskState state = NProto::DISK_STATE_ONLINE;

    for (const auto& uuid: disk.Devices) {
        const auto* device = DeviceList.FindDevice(uuid);

        if (!device) {
            return NProto::DISK_STATE_ERROR;
        }

        const auto* agent = AgentList.FindAgent(device->GetAgentId());
        if (!agent) {
            return NProto::DISK_STATE_ERROR;
        }

        state = std::max(state, ToDiskState(agent->GetState()));
        state = std::max(state, ToDiskState(device->GetState()));

        if (state == NProto::DISK_STATE_ERROR) {
            break;
        }
    }

    return state;
}

auto TDiskRegistryState::FindDeviceLocation(const TDeviceId& deviceId) const
    -> std::pair<const NProto::TAgentConfig*, const NProto::TDeviceConfig*>
{
    return const_cast<TDiskRegistryState*>(this)->FindDeviceLocation(deviceId);
}

auto TDiskRegistryState::FindDeviceLocation(const TDeviceId& deviceId)
    -> std::pair<NProto::TAgentConfig*, NProto::TDeviceConfig*>
{
    const auto agentId = DeviceList.FindAgentId(deviceId);
    if (agentId.empty()) {
        return {};
    }

    auto* agent = AgentList.FindAgent(agentId);
    if (!agent) {
        return {};
    }

    auto* device = FindIfPtr(*agent->MutableDevices(), [&] (const auto& x) {
        return x.GetDeviceUUID() == deviceId;
    });

    if (!device) {
        return {};
    }

    return {agent, device};
}

NProto::TError TDiskRegistryState::UpdateDeviceState(
    TDiskRegistryDatabase& db,
    const TString& deviceId,
    NProto::EDeviceState newState,
    TInstant now,
    TString reason,
    TDiskId& affectedDisk)
{
    auto error = CheckDeviceStateTransition(deviceId, newState, now);
    if (FAILED(error.GetCode())) {
        return error;
    }

    auto [agentPtr, devicePtr] = FindDeviceLocation(deviceId);
    if (!agentPtr || !devicePtr) {
        auto message = ReportDiskRegistryDeviceLocationNotFound(
            TStringBuilder() << "UpdateDeviceState:DeviceId: " << deviceId
            << ", agentPtr?: " << !!agentPtr
            << ", devicePtr?: " << !!devicePtr);

        return MakeError(E_FAIL, message);
    }

    const auto cmsTs = TInstant::MicroSeconds(devicePtr->GetCmsTs());
    const auto cmsDeadline = cmsTs + StorageConfig->GetNonReplicatedInfraTimeout();
    const auto cmsRequestActive = cmsTs && cmsDeadline > now;
    const auto oldState = devicePtr->GetState();

    if (!cmsRequestActive) {
        devicePtr->SetCmsTs(0);
    }

    // when newState is less than DEVICE_STATE_WARNING
    // check if device is not scheduled for shutdown by cms
    if (newState < NProto::DEVICE_STATE_WARNING && cmsRequestActive) {
        newState = oldState;
    }

    devicePtr->SetState(newState);
    devicePtr->SetStateTs(now.MicroSeconds());
    devicePtr->SetStateMessage(std::move(reason));

    ApplyDeviceStateChange(db, *agentPtr, *devicePtr, now, affectedDisk);

    return error;
}

NProto::TError TDiskRegistryState::UpdateCmsDeviceState(
    TDiskRegistryDatabase& db,
    const TString& deviceId,
    NProto::EDeviceState newState,
    TInstant now,
    bool dryRun,
    TDiskId& affectedDisk,
    TDuration& timeout)
{
    auto error = CheckDeviceStateTransition(deviceId, newState, now);
    if (FAILED(error.GetCode())) {
        return error;
    }

    auto [agentPtr, devicePtr] = FindDeviceLocation(deviceId);
    if (!agentPtr || !devicePtr) {
        auto message = ReportDiskRegistryDeviceLocationNotFound(
            TStringBuilder() << "UpdateCmsDeviceState:DeviceId: " << deviceId
            << ", agentPtr?: " << !!agentPtr
            << ", devicePtr?: " << !!devicePtr);

        return MakeError(E_FAIL, message);
    }

    NProto::TError result = MakeError(S_OK);
    TInstant cmsTs;
    timeout = TDuration::Zero();

    const auto diskId = FindDisk(devicePtr->GetDeviceUUID());
    const bool hasDependentDisk = diskId
        && devicePtr->GetState() < NProto::DEVICE_STATE_ERROR
        && newState != NProto::DEVICE_STATE_ONLINE;

    if (hasDependentDisk) {
        result = MakeError(
            E_TRY_AGAIN,
            TStringBuilder() << "have dependent disk: " << diskId);

        cmsTs = TInstant::MicroSeconds(devicePtr->GetCmsTs());
        if (cmsTs == TInstant::Zero()
                || cmsTs + StorageConfig->GetNonReplicatedInfraTimeout() <= now)
        {
            // restart timer
            cmsTs = now;
        }

        timeout = cmsTs + StorageConfig->GetNonReplicatedInfraTimeout() - now;
    }

    if (devicePtr->GetState() == NProto::DEVICE_STATE_ERROR) {
        // CMS can't return device from 'error' state.
        if (newState == NProto::DEVICE_STATE_ONLINE) {
            // Should retry while device is in 'error' state.
            result = MakeError(E_TRY_AGAIN, "device is in error state");
            if (timeout == TDuration::Zero()) {
                timeout = CMS_UPDATE_STATE_TO_ONLINE_TIMEOUT;
            }
        }
    }

    if (dryRun) {
        return result;
    }

    if (devicePtr->GetState() != NProto::DEVICE_STATE_ERROR) {
        devicePtr->SetState(newState);
        devicePtr->SetStateMessage("cms action");
    }
    devicePtr->SetStateTs(now.MicroSeconds());
    devicePtr->SetCmsTs(cmsTs.MicroSeconds());

    ApplyDeviceStateChange(db, *agentPtr, *devicePtr, now, affectedDisk);

    return result;
}

void TDiskRegistryState::ApplyDeviceStateChange(
    TDiskRegistryDatabase& db,
    const NProto::TAgentConfig& agent,
    const NProto::TDeviceConfig& device,
    TInstant now,
    TDiskId& affectedDisk)
{
    UpdateAgent(db, agent);
    DeviceList.UpdateDevices(agent);

    const auto& uuid = device.GetDeviceUUID();
    auto diskId = DeviceList.FindDiskId(uuid);

    if (diskId.empty()) {
        return;
    }

    auto* disk = Disks.FindPtr(diskId);

    if (!disk) {
        ReportDiskRegistryDiskNotFound(TStringBuilder()
            << "ApplyDeviceStateChange:DiskId: " << diskId);

        return;
    }

    // check if uuid is target for migration
    if (RestartDeviceMigration(db, diskId, *disk, uuid)) {
        return;
    }

    if (device.GetState() == NProto::DEVICE_STATE_ERROR
            && disk->MasterDiskId)
    {
        const bool canReplaceDevice = ReplicaTable.IsReplacementAllowed(
            disk->MasterDiskId,
            device.GetDeviceUUID());

        if (canReplaceDevice) {
            bool updated = false;
            auto error = ReplaceDevice(
                db,
                diskId,
                device.GetDeviceUUID(),
                "",     // no replacement device
                now,
                MakeMirroredDiskDeviceReplacementMessage(
                    disk->MasterDiskId,
                    "device failure"),
                false,  // manual
                &updated);

            if (HasError(error)) {
                ReportMirroredDiskDeviceReplacementFailure(
                    FormatError(error));
            }

            if (updated) {
                affectedDisk = diskId;
            }
        } else {
            ReportMirroredDiskDeviceReplacementForbidden();
        }

        return;
    }

    if (TryUpdateDiskState(db, diskId, *disk, now)) {
        affectedDisk = diskId;
    }

    if (device.GetState() != NProto::DEVICE_STATE_WARNING) {
        CancelDeviceMigration(db, diskId, *disk, uuid);
        return;
    }

    if (!disk->MigrationSource2Target.contains(uuid)) {
        AddMigration(*disk, diskId, uuid);
    }
}

bool TDiskRegistryState::RestartDeviceMigration(
    TDiskRegistryDatabase& db,
    const TDiskId& diskId,
    TDiskState& disk,
    const TDeviceId& targetId)
{
    auto it = disk.MigrationTarget2Source.find(targetId);

    if (it == disk.MigrationTarget2Source.end()) {
        return false;
    }

    TDeviceId sourceId = it->second;

    CancelDeviceMigration(db, diskId, disk, sourceId);

    AddMigration(disk, diskId, sourceId);

    return true;
}

void TDiskRegistryState::DeleteAllDeviceMigrations(const TDiskId& diskId)
{
    auto it = Migrations.lower_bound({diskId, TString {}});

    while (it != Migrations.end() && it->DiskId == diskId) {
        it = Migrations.erase(it);
    }
}

void TDiskRegistryState::DeleteDeviceMigration(
    const TDiskId& diskId,
    const TDeviceId& sourceId)
{
    Migrations.erase({ diskId, sourceId });
}

void TDiskRegistryState::CancelDeviceMigration(
    TDiskRegistryDatabase& db,
    const TDiskId& diskId,
    TDiskState& disk,
    const TDeviceId& sourceId)
{
    Migrations.erase(TDeviceMigration(diskId, sourceId));

    auto it = disk.MigrationSource2Target.find(sourceId);

    if (it == disk.MigrationSource2Target.end()) {
        return;
    }

    const TDeviceId targetId = it->second;

    disk.MigrationTarget2Source.erase(targetId);
    disk.MigrationSource2Target.erase(it);
    --DeviceMigrationsInProgress;

    const ui64 seqNo = AddReallocateRequest(db, diskId);

    disk.FinishedMigrations.push_back({
        .DeviceId = targetId,
        .SeqNo = seqNo
    });

    db.UpdateDisk(BuildDiskConfig(diskId, disk));

    UpdatePlacementGroup(db, diskId, disk, "CancelDeviceMigration");
}

NProto::TError TDiskRegistryState::FinishDeviceMigration(
    TDiskRegistryDatabase& db,
    const TDiskId& diskId,
    const TDeviceId& sourceId,
    const TDeviceId& targetId,
    TInstant timestamp,
    bool* diskStateUpdated)
{
    if (!Disks.contains(diskId)) {
        return MakeError(E_NOT_FOUND, TStringBuilder() <<
            "disk " << diskId.Quote() << " not found");
    }

    TDiskState& disk = Disks[diskId];

    auto devIt = Find(disk.Devices, sourceId);

    if (devIt == disk.Devices.end()) {
        // temporarily commented out, see NBS-3726
        // return MakeError(E_INVALID_STATE, TStringBuilder() <<
        //     "device " << sourceId.Quote() << " not found");
    }

    if (auto it = disk.MigrationTarget2Source.find(targetId);
        it == disk.MigrationTarget2Source.end() || it->second != sourceId)
    {
        return MakeError(E_ARGUMENT, "invalid migration");
    } else {
        disk.MigrationTarget2Source.erase(it);
        disk.MigrationSource2Target.erase(sourceId);
        --DeviceMigrationsInProgress;
    }

    const ui64 seqNo = AddReallocateRequest(db, diskId);
    // this condition is needed because of NBS-3726
    if (devIt != disk.Devices.end()) {
        *devIt = targetId;
        disk.FinishedMigrations.push_back({
            .DeviceId = sourceId,
            .SeqNo = seqNo
        });

        if (disk.MasterDiskId) {
            const bool replaced = ReplicaTable.ReplaceDevice(
                disk.MasterDiskId,
                sourceId,
                targetId);

            Y_VERIFY_DEBUG(replaced);
        }
    }

    *diskStateUpdated = TryUpdateDiskState(db, diskId, disk, timestamp);

    db.UpdateDisk(BuildDiskConfig(diskId, disk));

    UpdatePlacementGroup(db, diskId, disk, "FinishDeviceMigration");

    return {};
}

auto TDiskRegistryState::FindReplicaByMigration(
    const TDiskId& masterDiskId,
    const TDeviceId& sourceDeviceId,
    const TDeviceId& targetDeviceId) const -> TDiskId
{
    auto* masterDisk = Disks.FindPtr(masterDiskId);
    if (!masterDisk) {
        ReportDiskRegistryDiskNotFound(TStringBuilder()
            << "FindReplicaByMigration:MasterDiskId: " << masterDiskId);

        return {};
    }

    for (ui32 i = 0; i < masterDisk->ReplicaCount + 1; ++i) {
        auto replicaId = GetReplicaDiskId(masterDiskId, i);
        auto* replica = Disks.FindPtr(replicaId);

        if (!replica) {
            ReportDiskRegistryDiskNotFound(TStringBuilder()
                << "FindReplicaByMigration:ReplicaId: " << replicaId);

            return {};
        }

        auto* t = replica->MigrationSource2Target.FindPtr(sourceDeviceId);
        if (t && *t == targetDeviceId) {
            return replicaId;
        }
    }

    return {};
}

TVector<TDeviceMigration> TDiskRegistryState::BuildMigrationList() const
{
    size_t budget = StorageConfig->GetMaxNonReplicatedDeviceMigrationsInProgress();
    // can be true if we decrease the limit in our storage config while there
    // are more migrations in progress than our new limit
    if (budget <= DeviceMigrationsInProgress) {
        return {};
    }

    budget -= DeviceMigrationsInProgress;

    const size_t limit = std::min(Migrations.size(), budget);

    TVector<TDeviceMigration> result;
    result.reserve(limit);

    for (const auto& m: Migrations) {
        auto [agentPtr, devicePtr] = FindDeviceLocation(m.SourceDeviceId);
        if (!agentPtr || !devicePtr) {
            continue;
        }

        if (agentPtr->GetState() > NProto::AGENT_STATE_WARNING) {
            // skip migration from unavailable agents
            continue;
        }

        if (devicePtr->GetState() > NProto::DEVICE_STATE_WARNING) {
            // skip migration from broken devices
            continue;
        }

        result.push_back(m);

        if (result.size() == limit) {
            break;
        }
    }

    return result;
}

NProto::TError TDiskRegistryState::CheckDeviceStateTransition(
    const TString& deviceId,
    NProto::EDeviceState newState,
    TInstant timestamp)
{
    auto [agentPtr, devicePtr] = FindDeviceLocation(deviceId);
    if (!agentPtr || !devicePtr) {
        return MakeError(E_NOT_FOUND, TStringBuilder() <<
            "device " << deviceId.Quote() << " not found");
    }

    if (devicePtr->GetState() == newState) {
        return MakeError(S_ALREADY);
    }

    if (TInstant::MicroSeconds(devicePtr->GetStateTs()) > timestamp) {
        return MakeError(E_INVALID_STATE, "out of order");
    }

    return {};
}

TString TDiskRegistryState::GetAgentId(TNodeId nodeId) const
{
    const auto* agent = AgentList.FindAgent(nodeId);

    return agent
        ? agent->GetAgentId()
        : TString();
}

NProto::TDiskRegistryStateBackup TDiskRegistryState::BackupState() const
{
    static_assert(
        TTableCount<TDiskRegistrySchema::TTables>::value == 16,
        "not all fields are processed"
    );

    NProto::TDiskRegistryStateBackup backup;

    auto transform = [] (const auto& src, auto* dst, auto func) {
        dst->Reserve(src.size());
        std::transform(
            src.cbegin(),
            src.cend(),
            RepeatedFieldBackInserter(dst),
            func
        );
    };

    auto copy = [] (const auto& src, auto* dst) {
        dst->Reserve(src.size());
        std::copy(
            src.cbegin(),
            src.cend(),
            RepeatedFieldBackInserter(dst)
        );
    };

    auto copyMap = [] (const auto& src, auto* dst) {
        for (const auto& [k, v]: src) {
            (*dst)[k] = v;
        }
    };

    transform(Disks, backup.MutableDisks(), [this] (const auto& kv) {
        return BuildDiskConfig(kv.first, kv.second);
    });

    transform(PlacementGroups, backup.MutablePlacementGroups(), [] (const auto& kv) {
        return kv.second.Config;
    });

    transform(GetDirtyDevices(), backup.MutableDirtyDevices(), [this] (auto& x) {
        NProto::TDiskRegistryStateBackup::TDirtyDevice dd;
        dd.SetId(x.GetDeviceUUID());
        dd.SetDiskId(PendingCleanup.FindDiskId(x.GetDeviceUUID()));

        return dd;
    });

    transform(BrokenDisks, backup.MutableBrokenDisks(), [] (auto& x) {
        NProto::TDiskRegistryStateBackup::TBrokenDiskInfo info;
        info.SetDiskId(x.DiskId);
        info.SetTsToDestroy(x.TsToDestroy.MicroSeconds());

        return info;
    });

    transform(GetDisksToReallocate(), backup.MutableDisksToNotify(), [] (auto& kv) {
        return kv.first;
    });

    transform(GetDiskStateUpdates(), backup.MutableDiskStateChanges(), [] (auto& x) {
        NProto::TDiskRegistryStateBackup::TDiskStateUpdate update;

        update.MutableState()->CopyFrom(x.State);
        update.SetSeqNo(x.SeqNo);

        return update;
    });

    copy(AgentList.GetAgents(), backup.MutableAgents());
    copy(DisksToCleanup, backup.MutableDisksToCleanup());

    backup.MutableUserNotifications()->Reserve(GetUserNotifications().Count);
    for (auto&& [id, data]: GetUserNotifications().Storage) {
        bool doLegacyBackup = false;
        for (const auto& notif: data.Notifications) {
            if (notif.GetSeqNo() || !notif.GetHasLegacyCopy()) {
                *backup.AddUserNotifications() = notif;
            }

            if (notif.GetHasLegacyCopy()) {
                doLegacyBackup = true;
            }
        }

        if (doLegacyBackup) {
            backup.AddErrorNotifications(id);
        }
    }

    {
        auto outdatedVolumes = GetOutdatedVolumeConfigs();

        backup.MutableOutdatedVolumeConfigs()->Assign(
            std::make_move_iterator(outdatedVolumes.begin()),
            std::make_move_iterator(outdatedVolumes.end()));
    }

    copy(GetSuspendedDevices(), backup.MutableSuspendedDevices());

    transform(
        GetAutomaticallyReplacedDevices(),
        backup.MutableAutomaticallyReplacedDevices(),
        [] (auto& x) {
            NProto::TDiskRegistryStateBackup::TAutomaticallyReplacedDeviceInfo info;
            info.SetDeviceId(x.DeviceId);
            info.SetReplacementTs(x.ReplacementTs.MicroSeconds());

            return info;
        });

    copyMap(
        AgentList.GetDiskRegistryAgentListParams(),
        backup.MutableDiskRegistryAgentListParams());

    auto config = GetConfig();
    config.SetLastDiskStateSeqNo(NotificationSystem.GetDiskStateSeqNo());

    backup.MutableConfig()->Swap(&config);

    return backup;
}

bool TDiskRegistryState::IsReadyForCleanup(const TDiskId& diskId) const
{
    return DisksToCleanup.contains(diskId);
}

TVector<TString> TDiskRegistryState::GetDisksToCleanup() const
{
    return {DisksToCleanup.begin(), DisksToCleanup.end()};
}

std::pair<TVolumeConfig, ui64> TDiskRegistryState::GetVolumeConfigUpdate(
    const TDiskId& diskId) const
{
    auto seqNo = NotificationSystem.GetOutdatedVolumeSeqNo(diskId);
    if (!seqNo) {
        return {};
    }

    auto* disk = Disks.FindPtr(diskId);
    if (!disk) {
        return {};
    }

    TVolumeConfig config;

    config.SetDiskId(diskId);
    config.SetPlacementGroupId(disk->PlacementGroupId);
    config.SetPlacementPartitionIndex(disk->PlacementPartitionIndex);

    return {std::move(config), *seqNo};
}

auto TDiskRegistryState::GetOutdatedVolumeConfigs() const -> TVector<TDiskId>
{
    return NotificationSystem.GetOutdatedVolumeConfigs();
}

void TDiskRegistryState::DeleteOutdatedVolumeConfig(
    TDiskRegistryDatabase& db,
    const TDiskId& diskId)
{
    NotificationSystem.DeleteOutdatedVolumeConfig(db, diskId);
}

NProto::TError TDiskRegistryState::SetUserId(
    TDiskRegistryDatabase& db,
    const TDiskId& diskId,
    const TString& userId)
{
    auto it = Disks.find(diskId);
    if (it == Disks.end()) {
        return MakeError(E_NOT_FOUND, TStringBuilder() <<
            "disk " << diskId.Quote() << " not found");
    }

    auto& disk = it->second;
    disk.UserId = userId;

    db.UpdateDisk(BuildDiskConfig(diskId, disk));

    return {};
}

bool TDiskRegistryState::DoesNewDiskBlockSizeBreakDevice(
    const TDiskId& diskId,
    const TDeviceId& deviceId,
    ui64 newLogicalBlockSize)
{
    const auto& disk = Disks[diskId];
    const auto device = GetDevice(deviceId);

    const auto deviceSize = device.GetBlockSize() *
        GetDeviceBlockCountWithOverrides(diskId, device);

    const ui64 oldLogicalSize = deviceSize / disk.LogicalBlockSize
        * disk.LogicalBlockSize;
    const ui64 newLogicalSize = deviceSize / newLogicalBlockSize
        * newLogicalBlockSize;

    return oldLogicalSize != newLogicalSize;
}

NProto::TError TDiskRegistryState::ValidateUpdateDiskBlockSizeParams(
    const TDiskId& diskId,
    ui32 blockSize,
    bool force)
{
    if (diskId.empty()) {
        return MakeError(E_ARGUMENT, TStringBuilder() << "diskId is required");
    }

    if (blockSize == 0) {
        return MakeError(E_ARGUMENT, TStringBuilder()
            << "blockSize is required");
    }

    if (!Disks.contains(diskId)) {
        return MakeError(E_NOT_FOUND, TStringBuilder() <<
            "disk " << diskId.Quote() << " not found");
    }

    const auto& disk = Disks[diskId];

    if (blockSize == disk.LogicalBlockSize) {
        return MakeError(S_FALSE, TStringBuilder()
            << "disk " << diskId.Quote() << " already has block size equal to "
            << disk.LogicalBlockSize);
    }

    if (disk.Devices.empty()) {
        return MakeError(E_INVALID_STATE, "disk without devices");
    }

    if (force) {
        return {};
    }

    const auto forceNotice = "(use force flag to bypass this restriction)";

    ui64 devicesSize = 0;
    for (const auto& id: disk.Devices) {
        const auto device = GetDevice(id);
        if (device.GetDeviceUUID().empty()) {
            return MakeError(E_INVALID_STATE,
                TStringBuilder() << "one of the disk devices cannot be "
                "found " << forceNotice);
        }

        if (device.GetBlockSize() > blockSize) {
            return MakeError(E_ARGUMENT, TStringBuilder()
                << "volume's block size (" << blockSize
                << ") is less than device's block size ("
                << device.GetBlockSize() << ")");
        }

        devicesSize += device.GetBlocksCount() * device.GetBlockSize();
    }

    const auto error = ValidateBlockSize(blockSize, disk.MediaKind);

    if (HasError(error)) {
        return MakeError(error.GetCode(), TStringBuilder()
            << error.GetMessage() << " " << forceNotice);
    }

    const TString poolName = GetDevice(disk.Devices[0]).GetPoolName();
    const ui64 volumeSize = devicesSize / disk.LogicalBlockSize * blockSize;
    const ui64 allocationUnit = GetAllocationUnit(poolName);

    if (!allocationUnit) {
        return MakeError(E_ARGUMENT, TStringBuilder()
            << "zero allocation unit for pool: "
            << poolName);
    }

    if (volumeSize % allocationUnit != 0) {
        return MakeError(E_ARGUMENT, TStringBuilder()
            << "volume size should be divisible by " << allocationUnit);
    }

    for (const auto& deviceId: disk.Devices) {
        if (DoesNewDiskBlockSizeBreakDevice(diskId, deviceId, blockSize)) {
            return MakeError(E_ARGUMENT, TStringBuilder()
                << "Device " << deviceId.Quote()
                << " logical size " << disk.LogicalBlockSize
                << " is not equal to new logical size " << blockSize
                << ", that breaks disk " << forceNotice);
        }
    }

    return {};
}

NProto::TError TDiskRegistryState::UpdateDiskBlockSize(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TDiskId& diskId,
    ui32 blockSize,
    bool force)
{
    const auto validateError = ValidateUpdateDiskBlockSizeParams(diskId,
        blockSize, force);
    if (HasError(validateError) || validateError.GetCode() == S_FALSE) {
        return validateError;
    }

    auto& disk = Disks[diskId];

    for (const auto& deviceId: disk.Devices) {
        if (DoesNewDiskBlockSizeBreakDevice(diskId, deviceId, blockSize)) {
            auto device = GetDevice(deviceId);

            const auto newBlocksCount = device.GetBlockSize()
                * GetDeviceBlockCountWithOverrides(diskId, device)
                / disk.LogicalBlockSize * disk.LogicalBlockSize
                / device.GetBlockSize();
            AdjustDeviceBlockCount(now, db, device, newBlocksCount);
        }
    }

    disk.LogicalBlockSize = blockSize;
    UpdateAndReallocateDisk(db, diskId, disk);

    return {};
}

auto TDiskRegistryState::QueryAvailableStorage(
    const TString& agentId,
    const TString& poolName,
    NProto::EDevicePoolKind poolKind) const
        -> TResultOrError<TVector<TAgentStorageInfo>>
{
    if (!poolName.empty()) {
        auto* poolConfig = DevicePoolConfigs.FindPtr(poolName);
        if (!poolConfig) {
            return TVector<TAgentStorageInfo> {};
        }

        if (poolConfig->GetKind() != poolKind) {
            SelfCounters.QueryAvailableStorageErrors.Increment(1);

            return MakeError(
                E_ARGUMENT,
                Sprintf(
                    "Unexpected device pool kind (actual: %d, expected: %d) "
                    "for the device pool %s.",
                    poolKind,
                    poolConfig->GetKind(),
                    poolName.Quote().c_str()
                )
            );
        }
    }

    auto* agent = AgentList.FindAgent(agentId);
    if (!agent) {
        SelfCounters.QueryAvailableStorageErrors.Increment(1);

        return MakeError(E_NOT_FOUND, TStringBuilder{} <<
            "agent " << agentId.Quote() << " not found");
    }

    if (agent->GetState() != NProto::AGENT_STATE_ONLINE) {
        return TVector<TAgentStorageInfo> {};
    }

    THashMap<ui64, ui32> chunks;

    for (const auto& device: agent->GetDevices()) {
        if (device.GetPoolKind() != poolKind) {
            continue;
        }

        if (poolName && device.GetPoolName() != poolName) {
            continue;
        }

        if (device.GetState() != NProto::DEVICE_STATE_ONLINE) {
            continue;
        }

        if (DeviceList.IsSuspendedDevice(device.GetDeviceUUID())) {
            continue;
        }

        const ui64 au = GetAllocationUnit(device.GetPoolName());

        ++chunks[au];
    }

    TVector<TAgentStorageInfo> infos;
    infos.reserve(chunks.size());

    for (auto [size, count]: chunks) {
        infos.push_back({ size, count });
    }

    return infos;
}

NProto::TError TDiskRegistryState::AllocateDiskReplicas(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TDiskId& masterDiskId,
    ui32 replicaCount)
{
    if (replicaCount == 0) {
        return MakeError(E_ARGUMENT, "replica count can't be zero");
    }

    auto* masterDisk = Disks.FindPtr(masterDiskId);
    if (!masterDisk) {
        return MakeError(E_NOT_FOUND, TStringBuilder()
            << "disk " << masterDiskId.Quote() << " is not found");
    }

    if (masterDisk->ReplicaCount == 0) {
        return MakeError(E_ARGUMENT, "unable to allocate disk replica for not "
            "a master disk");
    }

    const auto newReplicaCount = masterDisk->ReplicaCount + replicaCount;

    const auto maxReplicaCount =
        StorageConfig->GetMaxDisksInPlacementGroup() - 1;
    if (newReplicaCount > maxReplicaCount) {
        return MakeError(E_ARGUMENT, TStringBuilder()
            << "mirrored disks can have maximum of " << maxReplicaCount
            << " replicas, and you are asking for " << newReplicaCount);
    }

    const auto firstReplicaDiskId = GetReplicaDiskId(masterDiskId, 0);
    const auto& firstReplica = Disks[firstReplicaDiskId];

    ui64 blocksCount = 0;
    for (const auto& deviceId: firstReplica.Devices) {
        const auto device = GetDevice(deviceId);
        blocksCount += device.GetBlockSize() * device.GetBlocksCount()
            / firstReplica.LogicalBlockSize;
    }

    auto onError = [&] (ui32 count) {
        for (ui32 i = 0; i < count; ++i) {
            const size_t idx = masterDisk->ReplicaCount + i + 1;

            PendingCleanup.Insert(
                masterDiskId,
                DeallocateSimpleDisk(
                    db,
                    GetReplicaDiskId(masterDiskId, idx),
                    "AllocateDiskReplicas:Cleanup")
            );
        }
    };

    const TAllocateDiskParams allocateParams {
        .DiskId = masterDiskId,
        .CloudId = firstReplica.CloudId,
        .FolderId = firstReplica.FolderId,
        .BlockSize = firstReplica.LogicalBlockSize,
        .BlocksCount = blocksCount,
    };

    for (size_t i = 0; i < replicaCount; ++i) {
        const size_t idx = masterDisk->ReplicaCount + i + 1;

        TAllocateDiskResult r;
        const auto error = AllocateDiskReplica(now, db, allocateParams, idx, &r);
        if (HasError(error)) {
            onError(i);

            return error;
        }

        // TODO (NBS-3418):
        // * add device ids for new replicas to device replacements
        // * update ReplicaTable
    }

    masterDisk->ReplicaCount = newReplicaCount;
    UpdateAndReallocateDisk(db, masterDiskId, *masterDisk);

    return {};
}

NProto::TError TDiskRegistryState::DeallocateDiskReplicas(
    TDiskRegistryDatabase& db,
    const TDiskId& masterDiskId,
    ui32 replicaCount)
{
    if (replicaCount == 0) {
        return MakeError(E_ARGUMENT, "replica count can't be zero");
    }

    auto* masterDisk = Disks.FindPtr(masterDiskId);
    if (!masterDisk) {
        return MakeError(E_NOT_FOUND, TStringBuilder()
            << "disk " << masterDiskId.Quote() << " is not found");
    }

    if (masterDisk->ReplicaCount == 0) {
        return MakeError(E_ARGUMENT, "unable to deallocate disk replica for "
            "not a master disk");
    }

    if (replicaCount > masterDisk->ReplicaCount) {
        return MakeError(E_ARGUMENT, TStringBuilder() << "deallocating "
            << replicaCount << "replicas is impossible, because disk "
            "only has " << masterDisk->ReplicaCount << " replicas");
    }
    if (replicaCount == masterDisk->ReplicaCount) {
        return MakeError(E_ARGUMENT, TStringBuilder() << "deallocating "
            << replicaCount << "replicas will make disk non mirrored, "
            "which is not supported yet");
    }

    const auto newReplicaCount = masterDisk->ReplicaCount - replicaCount;

    for (size_t i = masterDisk->ReplicaCount; i >= newReplicaCount + 1; --i) {
        const auto replicaDiskId = GetReplicaDiskId(masterDiskId, i);
        PendingCleanup.Insert(
            masterDiskId,
            DeallocateSimpleDisk(db, replicaDiskId, "DeallocateDiskReplicas")
        );

        // TODO (NBS-3418): update ReplicaTable
    }

    masterDisk->ReplicaCount = newReplicaCount;
    UpdateAndReallocateDisk(db, masterDiskId, *masterDisk);

    return {};
}

NProto::TError TDiskRegistryState::UpdateDiskReplicaCount(
    TDiskRegistryDatabase& db,
    const TDiskId& masterDiskId,
    ui32 replicaCount)
{
    const auto validateError = ValidateUpdateDiskReplicaCountParams(
        masterDiskId, replicaCount);
    if (HasError(validateError) || validateError.GetCode() == S_FALSE) {
        return validateError;
    }

    const auto& masterDisk = Disks[masterDiskId];

    if (replicaCount > masterDisk.ReplicaCount) {
        return AllocateDiskReplicas(Now(), db, masterDiskId,
            replicaCount - masterDisk.ReplicaCount);
    } else {
        return DeallocateDiskReplicas(db, masterDiskId,
            masterDisk.ReplicaCount - replicaCount);
    }
}

NProto::TError TDiskRegistryState::ValidateUpdateDiskReplicaCountParams(
    const TDiskId& masterDiskId,
    ui32 replicaCount)
{
    if (masterDiskId.empty()) {
        return MakeError(E_ARGUMENT, "disk id is required");
    }
    if (replicaCount == 0) {
        return MakeError(E_ARGUMENT,
            "unable to turn mirrored disk to a simple one");
    }

    const auto& masterDisk = Disks.FindPtr(masterDiskId);
    if (!masterDisk) {
        return MakeError(E_NOT_FOUND, TStringBuilder()
            << "disk " << masterDiskId.Quote() << " is not found");
    }

    const auto onlyMasterNotice = "The method only works with master disks";
    if (!masterDisk->MasterDiskId.empty()) {
        return MakeError(E_ARGUMENT, TStringBuilder()
            << "disk " << masterDiskId.Quote() << " is not a master, it's "
            "a slave of the master disk " << masterDisk->MasterDiskId.Quote() << ". "
            << onlyMasterNotice);
    }
    if (masterDisk->ReplicaCount == 0) {
        return MakeError(E_ARGUMENT, TStringBuilder()
            << "disk " << masterDiskId.Quote() << " is not a master. "
            << onlyMasterNotice);
    }

    if (replicaCount == masterDisk->ReplicaCount) {
        return MakeError(S_FALSE, TStringBuilder()
            << "disk " << masterDiskId.Quote() << " already has replicaCount "
            "equal to " << masterDisk->ReplicaCount << ", no changes will be made");
    }

    return {};
}

NProto::TError TDiskRegistryState::MarkReplacementDevice(
    TDiskRegistryDatabase& db,
    const TDiskId& diskId,
    const TDeviceId& deviceId,
    bool isReplacement)
{
    auto* disk = Disks.FindPtr(diskId);

    if (!disk) {
        return MakeError(
            E_NOT_FOUND,
            TStringBuilder() << "Disk " << diskId << " not found");
    }

    auto it = Find(disk->DeviceReplacementIds, deviceId);

    if (isReplacement) {
        if (it != disk->DeviceReplacementIds.end()) {
            return MakeError(
                S_ALREADY,
                TStringBuilder() << "Device " << deviceId
                    << " already in replacement list for disk " << diskId);
        }

        disk->DeviceReplacementIds.push_back(deviceId);
    } else {
        if (it == disk->DeviceReplacementIds.end()) {
            return MakeError(
                S_ALREADY,
                TStringBuilder() << "Device " << deviceId
                    << " not found in replacement list for disk " << diskId);
        }

        disk->DeviceReplacementIds.erase(it);
    }

    UpdateAndReallocateDisk(db, diskId, *disk);
    ReplicaTable.MarkReplacementDevice(diskId, deviceId, isReplacement);

    return {};
}

void TDiskRegistryState::UpdateAgent(
    TDiskRegistryDatabase& db,
    NProto::TAgentConfig config)
{
    if (config.GetNodeId() != 0) {
        db.UpdateOldAgent(config);
    }

    // don't persist unknown devices
    config.MutableUnknownDevices()->Clear();

    db.UpdateAgent(config);
}

ui64 TDiskRegistryState::GetAllocationUnit(const TString& poolName) const
{
    const auto* config = DevicePoolConfigs.FindPtr(poolName);
    if (!config) {
        return 0;
    }

    return config->GetAllocationUnit();
}

NProto::EDevicePoolKind TDiskRegistryState::GetDevicePoolKind(
    const TString& poolName) const
{
    const auto* config = DevicePoolConfigs.FindPtr(poolName);
    if (!config) {
        return NProto::DEVICE_POOL_KIND_DEFAULT;
    }

    return config->GetKind();
}

NProto::TError TDiskRegistryState::SuspendDevice(
    TDiskRegistryDatabase& db,
    const TDeviceId& id)
{
    if (id.empty()) {
        return MakeError(E_ARGUMENT, "empty device id");
    }

    DeviceList.SuspendDevice(id);

    NProto::TSuspendedDevice device;
    device.SetId(id);
    db.UpdateSuspendedDevice(std::move(device));

    return {};
}

void TDiskRegistryState::ResumeDevices(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TVector<TDeviceId>& ids)
{
    for (const auto& id: ids) {
        if (DeviceList.ResumeDevice(id)) {
            db.DeleteSuspendedDevice(id);

            TryUpdateDevice(now, db, id);
        } else {
            NProto::TSuspendedDevice device;
            device.SetId(id);
            device.SetResumeAfterErase(true);
            db.UpdateSuspendedDevice(device);
        }
    }
}

bool TDiskRegistryState::IsSuspendedDevice(const TDeviceId& id) const
{
    return DeviceList.IsSuspendedDevice(id);
}

TVector<NProto::TSuspendedDevice> TDiskRegistryState::GetSuspendedDevices() const
{
    return DeviceList.GetSuspendedDevices();
}

ui32 TDiskRegistryState::DeleteAutomaticallyReplacedDevices(
    TDiskRegistryDatabase& db,
    const TInstant until)
{
    auto it = AutomaticallyReplacedDevices.begin();
    ui32 cnt = 0;
    while (it != AutomaticallyReplacedDevices.end()
            && it->ReplacementTs <= until)
    {
        db.DeleteAutomaticallyReplacedDevice(it->DeviceId);
        AutomaticallyReplacedDeviceIds.erase(it->DeviceId);
        ++it;
        ++cnt;
    }
    AutomaticallyReplacedDevices.erase(AutomaticallyReplacedDevices.begin(), it);

    return cnt;
}

void TDiskRegistryState::DeleteAutomaticallyReplacedDevice(
    TDiskRegistryDatabase& db,
    const TDeviceId& deviceId)
{
    if (!AutomaticallyReplacedDeviceIds.erase(deviceId)) {
        return;
    }

    auto it = FindIf(AutomaticallyReplacedDevices, [&] (const auto& deviceInfo) {
        return (deviceInfo.DeviceId == deviceId);
    });
    if (it != AutomaticallyReplacedDevices.end()) {
        AutomaticallyReplacedDevices.erase(it);
    }

    db.DeleteAutomaticallyReplacedDevice(deviceId);
}

NProto::TError TDiskRegistryState::CreateDiskFromDevices(
    TInstant now,
    TDiskRegistryDatabase& db,
    bool force,
    const TDiskId& diskId,
    ui32 blockSize,
    const TVector<NProto::TDeviceConfig>& devices,
    TAllocateDiskResult* result)
{
    Y_VERIFY_DEBUG(result);

    if (devices.empty()) {
        return MakeError(E_ARGUMENT, "empty device list");
    }

    TVector<TString> deviceIds;
    int poolKind = -1;

    for (auto& device: devices) {
        auto [config, error] = FindDevice(device);
        if (HasError(error)) {
            return error;
        }

        if (blockSize < config.GetBlockSize()) {
            return MakeError(E_ARGUMENT, TStringBuilder() <<
                "volume's block size is less than device's block size: " <<
                blockSize << " < " << config.GetBlockSize());
        }

        const auto& uuid = config.GetDeviceUUID();

        if (poolKind == -1) {
            poolKind = static_cast<int>(config.GetPoolKind());
        }

        if (static_cast<int>(config.GetPoolKind()) != poolKind) {
            return MakeError(E_ARGUMENT, TStringBuilder() <<
                "several device pool kinds for one disk: " <<
                static_cast<int>(config.GetPoolKind()) << " and " <<
                poolKind);
        }

        if (!force
                && DeviceList.IsDirtyDevice(uuid)
                && !DeviceList.IsSuspendedDevice(uuid))
        {
            return MakeError(E_ARGUMENT, TStringBuilder() <<
                "device " << uuid.Quote() << " is dirty");
        }

        const auto otherDiskId = FindDisk(uuid);

        if (!otherDiskId.empty() && diskId != otherDiskId) {
            return MakeError(E_ARGUMENT, TStringBuilder() <<
                "device " << uuid.Quote() << " is allocated for disk "
                << otherDiskId.Quote());
        }

        deviceIds.push_back(uuid);
    }

    auto& disk = Disks[diskId];

    if (!disk.Devices.empty()) {
        if (disk.LogicalBlockSize == blockSize && disk.Devices == deviceIds) {
            for (const auto& uuid: disk.Devices) {
                const auto* device = DeviceList.FindDevice(uuid);
                if (!device) {
                    return MakeError(E_INVALID_STATE, TStringBuilder() <<
                        "device " << uuid.Quote() << " not found");
                }
                result->Devices.push_back(*device);
            }

            return MakeError(S_ALREADY);
        }

        return MakeError(E_ARGUMENT, TStringBuilder() <<
            "disk " << diskId.Quote() << " already exists");
    }

    disk.Devices = deviceIds;
    disk.LogicalBlockSize = blockSize;
    disk.StateTs = now;
    disk.State = CalculateDiskState(disk);

    for (auto& uuid: deviceIds) {
        DeviceList.MarkDeviceAllocated(diskId, uuid);
        DeviceList.MarkDeviceAsClean(uuid);
        db.DeleteDirtyDevice(uuid);

        DeviceList.ResumeDevice(uuid);
        db.DeleteSuspendedDevice(uuid);

        auto [agent, device] = FindDeviceLocation(uuid);

        STORAGE_VERIFY_C(
            agent && device,
            TWellKnownEntityTypes::DISK,
            diskId,
            "device " << uuid.Quote() << " not found");

        AdjustDeviceBlockCount(now, db, *device, device->GetUnadjustedBlockCount());

        result->Devices.push_back(*device);
    }

    db.UpdateDisk(BuildDiskConfig(diskId, disk));

    return {};
}

NProto::TError TDiskRegistryState::ChangeDiskDevice(
    TInstant now,
    TDiskRegistryDatabase& db,
    const TDiskId& diskId,
    const TDeviceId& sourceDeviceId,
    const TDeviceId& targetDeviceId)
{
    TDiskState* diskState = FindDiskState(diskId);
    if (!diskState) {
        return MakeError(E_ARGUMENT, TStringBuilder()
            <<  "disk " << diskId.Quote() << " not found");
    }

    TDeviceId* sourceDevicePtr = FindPtr(diskState->Devices, sourceDeviceId);
    if (!sourceDevicePtr) {
        return MakeError(E_ARGUMENT, TStringBuilder()
            << "disk " << diskId.Quote()
            << " didn't contain device " << sourceDeviceId.Quote());
    }

    auto* sourceDeviceConfig = DeviceList.FindDevice(sourceDeviceId);
    if (!sourceDeviceConfig) {
        return MakeError(E_ARGUMENT, TStringBuilder()
            << "source device " << sourceDeviceId.Quote() << " not found");
    }

    auto* targetDeviceConfig = DeviceList.FindDevice(targetDeviceId);
    if (!targetDeviceConfig) {
        return MakeError(E_ARGUMENT, TStringBuilder()
            << "target device " << targetDeviceId.Quote() << " not found");
    }

    auto targetDiskId = DeviceList.FindDiskId(targetDeviceId);
    if (targetDiskId) {
        return MakeError(E_ARGUMENT, TStringBuilder()
            << "device " << targetDeviceId.Quote()
            << " is allocated for disk " << targetDiskId.Quote());
    }

    if (sourceDeviceConfig->GetPoolKind()
        != targetDeviceConfig->GetPoolKind())
    {
        return MakeError(E_ARGUMENT, "Target and source pool kind not equal");
    }

    if (sourceDeviceConfig->GetBlockSize()
        != targetDeviceConfig->GetBlockSize())
    {
        return MakeError(E_ARGUMENT, "Target and source block size not equal");
    }

    *sourceDevicePtr = targetDeviceId;
    diskState->StateTs = now;
    diskState->State = CalculateDiskState(*diskState);

    DeviceList.MarkDeviceAllocated(diskId, targetDeviceId);
    DeviceList.MarkDeviceAsClean(targetDeviceId);
    db.DeleteDirtyDevice(targetDeviceId);

    DeviceList.ResumeDevice(targetDeviceId);
    db.DeleteSuspendedDevice(targetDeviceId);

    DeviceList.ReleaseDevice(sourceDeviceId);
    db.UpdateDirtyDevice(sourceDeviceId, diskId);

    auto [targetAgent, targetDevice] = FindDeviceLocation(targetDeviceId);
    STORAGE_VERIFY_C(
        targetAgent && targetDevice,
        TWellKnownEntityTypes::DISK,
        diskId,
        TStringBuilder()
            << "target device " << targetDeviceId.Quote() << " not found");
    targetDevice->SetState(NProto::DEVICE_STATE_ERROR);
    targetDevice->SetStateMessage("replaced by private api");
    targetDevice->SetStateTs(now.MicroSeconds());
    DeviceList.UpdateDevices(*targetAgent);
    UpdateAgent(db, *targetAgent);

    auto [sourceAgent, sourceDevice] = FindDeviceLocation(sourceDeviceId);
    STORAGE_VERIFY_C(
        sourceAgent && sourceDevice,
        TWellKnownEntityTypes::DISK,
        diskId,
        TStringBuilder()
            << "source device " << sourceDeviceId.Quote() << " not found");
    sourceDevice->SetState(NProto::DEVICE_STATE_ERROR);
    sourceDevice->SetStateMessage("replaced by private api");
    sourceDevice->SetStateTs(now.MicroSeconds());
    DeviceList.UpdateDevices(*sourceAgent);
    UpdateAgent(db, *sourceAgent);

    diskState->State = CalculateDiskState(*diskState);
    db.UpdateDisk(BuildDiskConfig(diskId, *diskState));

    AddReallocateRequest(db, diskId);

    return {};
}

const TVector<TDiskStateUpdate>& TDiskRegistryState::GetDiskStateUpdates() const
{
    return NotificationSystem.GetDiskStateUpdates();
}

void TDiskRegistryState::SetDiskRegistryAgentListParams(
    TDiskRegistryDatabase& db,
    const TString& agentId,
    const NProto::TDiskRegistryAgentParams& params)
{
    AgentList.SetDiskRegistryAgentListParams(agentId, params);
    db.AddDiskRegistryAgentListParams(agentId, params);
}

void TDiskRegistryState::CleanupExpiredAgentListParams(
    TDiskRegistryDatabase& db,
    TInstant now)
{
    for (const auto& agentId: AgentList.CleanupExpiredAgentListParams(now)) {
        db.DeleteDiskRegistryAgentListParams(agentId);
    }
}

TVector<TString> TDiskRegistryState::GetPoolNames() const
{
    TVector<TString> poolNames;
    for (const auto& [poolName, _]: DevicePoolConfigs) {
        poolNames.push_back(poolName);
    }
    Sort(poolNames);
    return poolNames;
}

}   // namespace NCloud::NBlockStore::NStorage
