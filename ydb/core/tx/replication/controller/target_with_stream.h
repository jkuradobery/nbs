#pragma once

#include "target_base.h"

namespace NKikimr {
namespace NReplication {
namespace NController {

class TTargetWithStream: public TTargetBase {
public:
    template <typename... Args>
    explicit TTargetWithStream(Args&&... args)
        : TTargetBase(std::forward<Args>(args)...)
    {
        SetStreamState(EStreamState::Creating);
    }

    void Progress(ui64 schemeShardId, const TActorId& proxy, const TActorContext& ctx) override;
    void Shutdown(const TActorContext& ctx) override;

private:
    bool NameAssignmentInProcess = false;
    TActorId StreamCreator;
    TActorId StreamRemover;

}; // TTargetWithStream

} // NController
} // NReplication
} // NKikimr
