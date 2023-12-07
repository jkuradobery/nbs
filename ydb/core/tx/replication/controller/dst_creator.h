#pragma once

#include "replication.h"

#include <ydb/core/base/defs.h>

namespace NKikimr {
namespace NReplication {
namespace NController {

IActor* CreateDstCreator(const TActorId& parent, ui64 schemeShardId, const TActorId& proxy,
    ui64 rid, ui64 tid, TReplication::ETargetKind kind, const TString& srcPath, const TString& dstPath);

} // NController
} // NReplication
} // NKikimr
