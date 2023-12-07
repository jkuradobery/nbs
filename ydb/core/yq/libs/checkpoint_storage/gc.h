#pragma once

#include "checkpoint_storage.h"
#include "state_storage.h"

#include <ydb/core/yq/libs/config/protos/checkpoint_coordinator.pb.h>

#include <library/cpp/actors/core/actor.h>

#include <memory>

namespace NYq {

////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<NActors::IActor> NewGC(
    const NConfig::TCheckpointGcConfig& config,
    const TCheckpointStoragePtr& checkpointStorage,
    const TStateStoragePtr& stateStorage);

} // namespace NYq
