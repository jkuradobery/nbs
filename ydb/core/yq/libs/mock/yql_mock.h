#pragma once

#include <ydb/core/testlib/actors/test_runtime.h>
#include <ydb/core/yq/libs/shared_resources/interface/shared_resources.h>

#include <library/cpp/actors/core/actorsystem.h>

namespace NYq {

NActors::IActor* CreateYqlMockActor(int grpcPort);
void InitTest(NActors::TTestActorRuntime* runtime, int httpPort, int grpcPort, const IYqSharedResources::TPtr& yqSharedResources);

} // namespace NYq
