#pragma once

#include <ydb/core/yq/libs/actors/logging/log.h>
#include <ydb/core/yq/libs/config/protos/control_plane_storage.pb.h>
#include <ydb/core/yq/libs/shared_resources/shared_resources.h>

#include <library/cpp/actors/core/actor.h>
#include <library/cpp/monlib/dynamic_counters/counters.h>


#define CPC_LOG_D(s) \
    LOG_FQ_CONTROL_PLANE_CONFIG_DEBUG(s)
#define CPC_LOG_I(s) \
    LOG_FQ_CONTROL_PLANE_CONFIG_INFO(s)
#define CPC_LOG_W(s) \
    LOG_FQ_CONTROL_PLANE_CONFIG_WARN(s)
#define CPC_LOG_E(s) \
    LOG_FQ_CONTROL_PLANE_CONFIG_ERROR(s)
#define CPC_LOG_T(s) \
    LOG_FQ_CONTROL_PLANE_CONFIG_TRACE(s)

namespace NYq {

NActors::TActorId ControlPlaneConfigActorId();

NActors::IActor* CreateControlPlaneConfigActor(const ::NYq::TYqSharedResources::TPtr& yqSharedResources, const NKikimr::TYdbCredentialsProviderFactory& credProviderFactory, const NConfig::TControlPlaneStorageConfig& config, const ::NMonitoring::TDynamicCounterPtr& counters);

} // namespace NYq
