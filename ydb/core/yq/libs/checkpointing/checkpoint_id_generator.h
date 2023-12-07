#pragma once

#include <ydb/core/yq/libs/checkpointing_common/defs.h>

namespace NYq {

class TCheckpointIdGenerator {
private:
    TCoordinatorId CoordinatorId;
    ui64 NextNumber;

public:
    explicit TCheckpointIdGenerator(TCoordinatorId id, TCheckpointId lastCheckpoint = TCheckpointId(0, 0));

    TCheckpointId NextId();
};

} // namespace NYq
