#include "volume_database.h"

#include "volume_schema.h"

#include <cloud/blockstore/libs/service/request_helpers.h>
#include <cloud/blockstore/public/api/protos/mount.pb.h>

namespace NCloud::NBlockStore::NStorage {

using namespace NKikimr;

namespace {

////////////////////////////////////////////////////////////////////////////////

constexpr ui32 META_KEY = 1;
constexpr ui32 THROTTLER_STATE_KEY = 1;

////////////////////////////////////////////////////////////////////////////////

TInstant ConvertHistoryTime(ui64 ts)
{
    return TInstant::MicroSeconds(Max<ui64>() - ts);
}

ui64 ConvertHistoryTime(TInstant ts)
{
    return Max<ui64>() - ts.MicroSeconds();
}

};  // namespace

////////////////////////////////////////////////////////////////////////////////

void TVolumeDatabase::InitSchema()
{
    Materialize<TVolumeSchema>();

    TSchemaInitializer<TVolumeSchema::TTables>::InitStorage(Database.Alter());
}

////////////////////////////////////////////////////////////////////////////////

void TVolumeDatabase::WriteMeta(const NProto::TVolumeMeta& meta)
{
    using TTable = TVolumeSchema::Meta;

    Table<TTable>()
        .Key(META_KEY)
        .Update(NIceDb::TUpdate<TTable::VolumeMeta>(meta));
}

bool TVolumeDatabase::ReadMeta(TMaybe<NProto::TVolumeMeta>& meta)
{
    using TTable = TVolumeSchema::Meta;

    auto it = Table<TTable>()
        .Key(META_KEY)
        .Select<TTable::TColumns>();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    if (it.IsValid()) {
        meta = it.GetValue<TTable::VolumeMeta>();
    }

    return true;
}

void TVolumeDatabase::WriteStartPartitionsNeeded(const bool startPartitionsNeeded)
{
    using TTable = TVolumeSchema::Meta;

    Table<TTable>()
        .Key(META_KEY)
        .Update(NIceDb::TUpdate<TTable::StartPartitionsNeeded>(startPartitionsNeeded));
}

bool TVolumeDatabase::ReadStartPartitionsNeeded(TMaybe<bool>& startPartitionsNeeded)
{
    using TTable = TVolumeSchema::Meta;

    auto it = Table<TTable>()
        .Key(META_KEY)
        .Select<TTable::TColumns>();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    if (it.IsValid()) {
        startPartitionsNeeded = it.GetValue<TTable::StartPartitionsNeeded>();
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

void TVolumeDatabase::WriteMetaHistory(
    ui32 version,
    const TVolumeMetaHistoryItem& meta)
{
    using TTable = TVolumeSchema::MetaHistory;

    Table<TTable>()
        .Key(version)
        .Update(
            NIceDb::TUpdate<TTable::Timestamp>(meta.Timestamp.MicroSeconds()),
            NIceDb::TUpdate<TTable::VolumeMeta>(meta.Meta));

    // simple size limit
    Table<TTable>()
        .Key(version - 1000)
        .Delete();
}

bool TVolumeDatabase::ReadMetaHistory(TVector<TVolumeMetaHistoryItem>& metas)
{
    using TTable = TVolumeSchema::MetaHistory;

    auto it = Table<TTable>()
        .Range()
        .Select<TTable::TColumns>();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    while (it.IsValid()) {
        auto timestamp = TInstant::MicroSeconds(it.GetValue<TTable::Timestamp>());
        auto meta = it.GetValue<TTable::VolumeMeta>();
        metas.push_back({timestamp, std::move(meta)});

        if (!it.Next()) {
            return false;   // not ready
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

void TVolumeDatabase::WriteClients(const THashMap<TString, TVolumeClientState>& infos)
{
    using TTable = TVolumeSchema::Clients;

    for (const auto& pair: infos) {
        Table<TTable>()
            .Key(pair.first)
            .Update(NIceDb::TUpdate<TTable::ClientInfo>(pair.second.GetVolumeClientInfo()));
    }
}

bool TVolumeDatabase::ReadClients(THashMap<TString, TVolumeClientState>& infos)
{
    using TTable = TVolumeSchema::Clients;

    auto it = Table<TTable>()
        .Range()
        .Select<TTable::TColumns>();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    while (it.IsValid()) {
        auto info = it.GetValue<TTable::ClientInfo>();
        infos[info.GetClientId()] = TVolumeClientState(info);

        if (!it.Next()) {
            return false;   // not ready
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

void TVolumeDatabase::WriteClient(const NProto::TVolumeClientInfo& info)
{
    using TTable = TVolumeSchema::Clients;

    Table<TTable>()
        .Key(info.GetClientId())
        .Update(NIceDb::TUpdate<TTable::ClientInfo>(info));
}

bool TVolumeDatabase::ReadClient(
    const TString& clientId,
    TMaybe<NProto::TVolumeClientInfo>& info)
{
    using TTable = TVolumeSchema::Clients;

    auto it = Table<TTable>()
        .Key(clientId)
        .Select<TTable::TColumns>();

    if (!it.IsReady()) {
        return false;
    }

    if (it.IsValid()) {
        info = it.GetValue<TTable::ClientInfo>();
    }

    return true;
}

void TVolumeDatabase::RemoveClient(const TString& clientId)
{
    using TTable = TVolumeSchema::Clients;

    Table<TTable>()
        .Key(clientId)
        .Delete();
}

////////////////////////////////////////////////////////////////////////////////

bool TVolumeDatabase::ReadOutdatedHistory(
    TVector<THistoryLogKey>& records,
    TInstant oldestTimestamp)
{
    using TTable = TVolumeSchema::History;

    auto it = Table<TTable>()
        .GreaterOrEqual(ConvertHistoryTime(oldestTimestamp), 0)
        .Select<TTable::TKeyColumns>();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    while (it.IsValid()) {
        THistoryLogKey key {
            ConvertHistoryTime(it.GetValue<TTable::Timestamp>()),
            it.GetValue<TTable::SeqNo>()};
        records.push_back(key);

        if (!it.Next()) {
            return false;   // not ready
        }
    }

    return true;
}

bool TVolumeDatabase::ReadHistory(
    TDeque<THistoryLogItem>& records,
    TInstant startTs,
    TInstant endTs,
    ui64 numRecords)
{
    TVector<THistoryLogItem> out;
    bool result = ReadHistory(
        out,
        startTs,
        endTs,
        numRecords);

    if (result) {
        for (auto& h : out) {
            records.push_back(std::move(h));
        }
    }
    return result;
}

bool TVolumeDatabase::ReadHistory(
    TVector<THistoryLogItem>& records,
    TInstant startTs,
    TInstant endTs,
    ui64 numRecords)
{
    using TTable = TVolumeSchema::History;

    auto it = Table<TTable>()
        .GreaterOrEqual(ConvertHistoryTime(startTs), 0)
        .Select<TTable::TColumns>();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    while (it.IsValid()) {
        THistoryLogKey itemKey;
        itemKey.Timestamp = ConvertHistoryTime(it.template GetValue<TTable::Timestamp>());
        if (itemKey.Timestamp < endTs) {
            return true;
        }

        itemKey.Seqno = it.template GetValue<TTable::SeqNo>();

        THistoryLogItem item {itemKey, it.template GetValue<TTable::OperationInfo>()};

        if (numRecords &&
            records.size() >= numRecords &&
            itemKey.Timestamp != records.back().Key.Timestamp)
        {
            return true;
        }

        records.push_back(std::move(item));

        if (!it.Next()) {
            return false;   // not ready
        }
    }
    return true;
}

void TVolumeDatabase::DeleteHistoryEntry(THistoryLogKey entry)
{
    using TTable = TVolumeSchema::History;

    Table<TTable>()
        .Key(ConvertHistoryTime(entry.Timestamp), entry.Seqno)
        .Delete();
}

void TVolumeDatabase::WriteHistory(THistoryLogItem item)
{
    using TTable = TVolumeSchema::History;
    Table<TTable>()
        .Key(ConvertHistoryTime(
            item.Key.Timestamp),
            item.Key.Seqno)
        .Update(NIceDb::TUpdate<TTable::OperationInfo>(item.Operation));
}

////////////////////////////////////////////////////////////////////////////////

void TVolumeDatabase::WritePartStats(
    ui64 partTabletId,
    const NProto::TCachedPartStats& stats)
{
    using TTable = TVolumeSchema::PartStats;

    Table<TTable>()
        .Key(partTabletId)
        .Update(NIceDb::TUpdate<TTable::Stats>(stats));
}

bool TVolumeDatabase::ReadPartStats(TVector<TPartStats>& stats)
{
    using TTable = TVolumeSchema::PartStats;

    auto it = Table<TTable>().Select<TTable::TColumns>();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    while (it.IsValid()) {
        stats.push_back({
            it.GetValue<TTable::PartTabletId>(),
            it.GetValue<TTable::Stats>()
        });

        if (!it.Next()) {
            return false;   // not ready
        }
    }

    return true;
}

void TVolumeDatabase::WriteNonReplPartStats(
    ui64 id,
    const NProto::TCachedPartStats& stats)
{
    using TTable = TVolumeSchema::NonReplPartStats;

    Table<TTable>()
        .Key(id)
        .Update(NIceDb::TUpdate<TTable::Stats>(stats));
}

bool TVolumeDatabase::ReadNonReplPartStats(TVector<TPartStats>& stats)
{
    using TTable = TVolumeSchema::NonReplPartStats;

    auto it = Table<TTable>().Select<TTable::TColumns>();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    while (it.IsValid()) {
        stats.push_back({
            it.GetValue<TTable::Id>(),
            it.GetValue<TTable::Stats>()
        });

        if (!it.Next()) {
            return false;   // not ready
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

void TVolumeDatabase::WriteCheckpointRequest(
    const TCheckpointRequest& request)
{
    using TTable = TVolumeSchema::CheckpointRequests;

    Table<TTable>()
        .Key(request.RequestId)
        .Update(
            NIceDb::TUpdate<TTable::CheckpointId>(request.CheckpointId),
            NIceDb::TUpdate<TTable::Timestamp>(request.Timestamp.MicroSeconds()),
            NIceDb::TUpdate<TTable::State>(static_cast<ui32>(request.State)),
            NIceDb::TUpdate<TTable::ReqType>(static_cast<ui32>(request.ReqType)),
            NIceDb::TUpdate<TTable::CheckpointType>(static_cast<ui32>(request.Type)));
}

void TVolumeDatabase::UpdateCheckpointRequest(ui64 requestId, bool completed)
{
    using TTable = TVolumeSchema::CheckpointRequests;

    auto state = completed ? ECheckpointRequestState::Completed
                           : ECheckpointRequestState::Rejected;
    Table<TTable>()
        .Key(requestId)
        .Update(
            NIceDb::TUpdate<TTable::State>(static_cast<ui32>(state))
        );
}

bool TVolumeDatabase::CollectCheckpointsToDelete(
    TDuration deletedCheckpointHistoryLifetime,
    TInstant now,
    THashMap<TString, TInstant>& deletedCheckpoints)
{
    using TTable = TVolumeSchema::CheckpointRequests;

    auto it = Table<TTable>().Select<TTable::TColumns>();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    while (it.IsValid()) {
        auto requestState = static_cast<ECheckpointRequestState>(it.GetValue<TTable::State>());
        auto timestamp = TInstant::MicroSeconds(it.GetValue<TTable::Timestamp>());
        auto reqType = static_cast<ECheckpointRequestType>(it.GetValue<TTable::ReqType>());

        if (reqType == ECheckpointRequestType::Delete &&
            requestState == ECheckpointRequestState::Completed &&
            timestamp + deletedCheckpointHistoryLifetime <= now)
        {
            deletedCheckpoints[it.GetValue<TTable::CheckpointId>()] = timestamp;
        }

        if (!it.Next()) {
            return false;   // not ready
        }
    }

    return true;
}


bool TVolumeDatabase::ReadCheckpointRequests(
    const THashMap<TString, TInstant>& deletedCheckpoints,
    TVector<TCheckpointRequest>& requests,
    TVector<ui64>& outdatedCheckpointRequestIds)
{
    using TTable = TVolumeSchema::CheckpointRequests;

    auto it = Table<TTable>().Select<TTable::TColumns>();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    while (it.IsValid()) {
        const auto& id = it.GetValue<TTable::CheckpointId>();
        auto checkpointIt = deletedCheckpoints.find(id);
        if (checkpointIt != deletedCheckpoints.end() &&
            TInstant::MicroSeconds(it.GetValue<TTable::Timestamp>()) <= checkpointIt->second)
        {
            outdatedCheckpointRequestIds.push_back(it.GetValue<TTable::RequestId>());
        } else {
            requests.emplace_back(
                it.GetValue<TTable::RequestId>(),
                it.GetValue<TTable::CheckpointId>(),
                TInstant::MicroSeconds(it.GetValue<TTable::Timestamp>()),
                static_cast<ECheckpointRequestType>(it.GetValue<TTable::ReqType>()),
                static_cast<ECheckpointRequestState>(it.GetValue<TTable::State>()),
                static_cast<ECheckpointType>(it.GetValue<TTable::CheckpointType>()));
        }

        if (!it.Next()) {
            return false;   // not ready
        }
    }

    return true;
}

void TVolumeDatabase::DeleteCheckpointEntry(ui64 requestId)
{
    using TTable = TVolumeSchema::CheckpointRequests;

    Table<TTable>()
        .Key(requestId)
        .Delete();
}

////////////////////////////////////////////////////////////////////////////////

void TVolumeDatabase::WriteUsedBlocks(
    const TCompressedBitmap::TSerializedChunk& chunk)
{
    using TTable = TVolumeSchema::UsedBlocks;

    Table<TTable>()
        .Key(chunk.ChunkIdx)
        .Update(NIceDb::TUpdate<TTable::Bitmap>(chunk.Data));
}

bool TVolumeDatabase::ReadUsedBlocks(TCompressedBitmap& usedBlocks)
{
    using TTable = TVolumeSchema::UsedBlocks;

    auto it = Table<TTable>()
        .Range()
        .Select();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    while (it.IsValid()) {
        usedBlocks.Update({
            it.GetValue<TTable::RangeIndex>(),
            it.GetValue<TTable::Bitmap>()
        });

        if (!it.Next()) {
            return false;   // not ready
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

void TVolumeDatabase::WriteThrottlerState(const TThrottlerStateInfo& stateInfo)
{
    using TTable = TVolumeSchema::ThrottlerState;

    Table<TTable>()
        .Key(THROTTLER_STATE_KEY)
        .Update(NIceDb::TUpdate<TTable::Budget>(stateInfo.Budget));
}

bool TVolumeDatabase::ReadThrottlerState(TMaybe<TThrottlerStateInfo>& stateInfo)
{
    using TTable = TVolumeSchema::ThrottlerState;

    auto it = Table<TTable>()
        .Key(THROTTLER_STATE_KEY)
        .Select<TTable::TColumns>();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    if (it.IsValid()) {
        stateInfo = {
            it.GetValue<TTable::Budget>()
        };
    }

    return true;
}


///////////////////////////////////////////////////////

void TVolumeDatabase::WriteVolumeParams(
    const TVector<TVolumeParamsValue>& volumeParams)
{
    using TTable = TVolumeSchema::VolumeParams;

    for (const auto& param: volumeParams) {
        Table<TTable>()
                .Key(param.Key)
                .Update(
                    NIceDb::TUpdate<TTable::Value>(param.Value),
                    NIceDb::TUpdate<TTable::ValidUntil>(param.ValidUntil.MicroSeconds())
                );
    }
}

bool TVolumeDatabase::ReadVolumeParams(TVector<TVolumeParamsValue>& volumeParams) {
    using TTable = TVolumeSchema::VolumeParams;

    volumeParams.clear();

    auto it = Table<TTable>()
        .Range()
        .Select<TTable::TColumns>();

    if (!it.IsReady()) {
        return false;   // not ready
    }

    while (it.IsValid()) {
        volumeParams.push_back({
            it.GetValue<TTable::Key>(),
            it.GetValue<TTable::Value>(),
            TInstant::MicroSeconds(
                it.GetValue<TTable::ValidUntil>()
            )
        });

        if (!it.Next()) {
            return false;   // not ready
        }
    }

    return true;
}

}   // namespace NCloud::NBlockStore::NStorage
