#include "private_events.h"
#include "stream_creator.h"
#include "target_with_stream.h"

#include <library/cpp/actors/core/events.h>

namespace NKikimr {
namespace NReplication {
namespace NController {

void TTargetWithStream::Progress(ui64 schemeShardId, const TActorId& proxy, const TActorContext& ctx) {
    switch (GetStreamState()) {
    case EStreamState::Creating:
        if (GetStreamName().empty() && !NameAssignmentInProcess) {
            ctx.Send(ctx.SelfID, new TEvPrivate::TEvAssignStreamName(GetReplicationId(), GetTargetId()));
            NameAssignmentInProcess = true;
        } else if (!StreamCreator) {
            StreamCreator = ctx.Register(CreateStreamCreator(ctx.SelfID, GetReplicationId(), GetTargetId(), proxy));
        }
        return;
    case EStreamState::Removing:
        return; // TODO
    case EStreamState::Ready:
    case EStreamState::Error:
        break;
    }

    TTargetBase::Progress(schemeShardId, proxy, ctx);
}

void TTargetWithStream::Shutdown(const TActorContext& ctx) {
    for (auto& x : TVector<TActorId>{StreamCreator, StreamRemover}) {
        if (auto actorId = std::exchange(x, {})) {
            ctx.Send(actorId, new TEvents::TEvPoison());
        }
    }

    TTargetBase::Shutdown(ctx);
}

} // NController
} // NReplication
} // NKikimr
