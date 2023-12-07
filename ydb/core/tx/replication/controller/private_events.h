#pragma once

#include <ydb/public/sdk/cpp/client/ydb_scheme/scheme.h>

#include <ydb/core/base/defs.h>
#include <ydb/core/base/events.h>
#include <ydb/core/base/pathid.h>
#include <ydb/core/protos/flat_tx_scheme.pb.h>

namespace NKikimr {
namespace NReplication {
namespace NController {

struct TEvPrivate {
    enum EEv {
        EvDiscoveryResult = EventSpaceBegin(TKikimrEvents::ES_PRIVATE),
        EvAssignStreamName,
        EvCreateStreamResult,
        EvCreateDstResult,

        EvEnd,
    };

    static_assert(EvEnd < EventSpaceEnd(TKikimrEvents::ES_PRIVATE), "expect EvEnd < EventSpaceEnd(TKikimrEvents::ES_PRIVATE)");

    struct TEvDiscoveryResult: public TEventLocal<TEvDiscoveryResult, EvDiscoveryResult> {
        using TAddEntry = std::pair<NYdb::NScheme::TSchemeEntry, TString>; // src, dst
        using TFailedEntry = std::pair<TString, NYdb::TStatus>; // src, error

        const ui64 ReplicationId;
        TVector<TAddEntry> ToAdd;
        TVector<ui64> ToDelete;
        TVector<TFailedEntry> Failed;

        explicit TEvDiscoveryResult(ui64 rid, TVector<TAddEntry>&& toAdd, TVector<ui64>&& toDel);
        explicit TEvDiscoveryResult(ui64 rid, TVector<TFailedEntry>&& failed);
        TString ToString() const override;

        bool IsSuccess() const;
    };

    struct TEvAssignStreamName: public TEventLocal<TEvAssignStreamName, EvAssignStreamName> {
        const ui64 ReplicationId;
        const ui64 TargetId;

        explicit TEvAssignStreamName(ui64 rid, ui64 tid);
        TString ToString() const override;
    };

    struct TEvCreateStreamResult: public TEventLocal<TEvCreateStreamResult, EvCreateStreamResult> {
        const ui64 ReplicationId;
        const ui64 TargetId;
        const NYdb::TStatus Status;

        explicit TEvCreateStreamResult(ui64 rid, ui64 tid, NYdb::TStatus&& status);
        TString ToString() const override;

        bool IsSuccess() const;
    };

    struct TEvCreateDstResult: public TEventLocal<TEvCreateDstResult, EvCreateDstResult> {
        const ui64 ReplicationId;
        const ui64 TargetId;
        const TPathId DstPathId;
        const NKikimrScheme::EStatus Status;
        const TString Error;

        explicit TEvCreateDstResult(ui64 rid, ui64 tid, const TPathId& dstPathId);
        explicit TEvCreateDstResult(ui64 rid, ui64 tid, NKikimrScheme::EStatus status, const TString& error);
        TString ToString() const override;

        bool IsSuccess() const;
    };

}; // TEvPrivate

} // NController
} // NReplication
} // NKikimr
