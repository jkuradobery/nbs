#pragma once
#include <library/cpp/actors/core/actorsystem.h>

#include <util/generic/ptr.h>

namespace NYq {

struct IYqSharedResources : public TThrRefBase {
    using TPtr = TIntrusivePtr<IYqSharedResources>;

    virtual void Init(NActors::TActorSystem* actorSystem) = 0;

    // Called after actor system stop.
    virtual void Stop() = 0;
};

} // NYq
