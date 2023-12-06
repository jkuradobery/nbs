#pragma once
#include <ydb/library/services/services.pb.h>

#include <ydb/library/actors/core/log.h>

#define LOG_STREAMS_IMPL(level, component, logRecordStream) \
    LOG_LOG_S(::NActors::TActivationContext::AsActorContext(), ::NActors::NLog:: Y_CAT(PRI_, level), ::NKikimrServices::component, logRecordStream);

#define LOG_STREAMS_IMPL_AS(actorSystem, level, component, logRecordStream) \
    LOG_LOG_S(actorSystem, ::NActors::NLog:: Y_CAT(PRI_, level), ::NKikimrServices::component, logRecordStream);

// Component: STREAMS.
#define LOG_STREAMS_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, STREAMS, logRecordStream)
#define LOG_STREAMS_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, STREAMS, logRecordStream)
#define LOG_STREAMS_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, STREAMS, logRecordStream)
#define LOG_STREAMS_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, STREAMS, logRecordStream)
#define LOG_STREAMS_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, STREAMS, logRecordStream)
#define LOG_STREAMS_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, STREAMS, logRecordStream)
#define LOG_STREAMS_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, STREAMS, logRecordStream)
#define LOG_STREAMS_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, STREAMS, logRecordStream)
#define LOG_STREAMS_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, STREAMS, logRecordStream)

// Component: STREAMS_SERVICE.
#define LOG_STREAMS_SERVICE_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, STREAMS_SERVICE, logRecordStream)
#define LOG_STREAMS_SERVICE_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, STREAMS_SERVICE, logRecordStream)
#define LOG_STREAMS_SERVICE_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, STREAMS_SERVICE, logRecordStream)
#define LOG_STREAMS_SERVICE_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, STREAMS_SERVICE, logRecordStream)
#define LOG_STREAMS_SERVICE_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, STREAMS_SERVICE, logRecordStream)
#define LOG_STREAMS_SERVICE_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, STREAMS_SERVICE, logRecordStream)
#define LOG_STREAMS_SERVICE_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, STREAMS_SERVICE, logRecordStream)
#define LOG_STREAMS_SERVICE_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, STREAMS_SERVICE, logRecordStream)
#define LOG_STREAMS_SERVICE_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, STREAMS_SERVICE, logRecordStream)

// Component: STREAMS_STORAGE_SERVICE.
#define LOG_STREAMS_STORAGE_SERVICE_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, STREAMS_STORAGE_SERVICE, logRecordStream)
#define LOG_STREAMS_STORAGE_SERVICE_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, STREAMS_STORAGE_SERVICE, logRecordStream)
#define LOG_STREAMS_STORAGE_SERVICE_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, STREAMS_STORAGE_SERVICE, logRecordStream)
#define LOG_STREAMS_STORAGE_SERVICE_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, STREAMS_STORAGE_SERVICE, logRecordStream)
#define LOG_STREAMS_STORAGE_SERVICE_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, STREAMS_STORAGE_SERVICE, logRecordStream)
#define LOG_STREAMS_STORAGE_SERVICE_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, STREAMS_STORAGE_SERVICE, logRecordStream)
#define LOG_STREAMS_STORAGE_SERVICE_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, STREAMS_STORAGE_SERVICE, logRecordStream)
#define LOG_STREAMS_STORAGE_SERVICE_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, STREAMS_STORAGE_SERVICE, logRecordStream)
#define LOG_STREAMS_STORAGE_SERVICE_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, STREAMS_STORAGE_SERVICE, logRecordStream)

#define LOG_STREAMS_STORAGE_SERVICE_AS_DEBUG(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, DEBUG, STREAMS_STORAGE_SERVICE, logRecordStream)

// Component: STREAMS_SCHEDULER_SERVICE.
#define LOG_STREAMS_SCHEDULER_SERVICE_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, STREAMS_SCHEDULER_SERVICE, logRecordStream)
#define LOG_STREAMS_SCHEDULER_SERVICE_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, STREAMS_SCHEDULER_SERVICE, logRecordStream)
#define LOG_STREAMS_SCHEDULER_SERVICE_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, STREAMS_SCHEDULER_SERVICE, logRecordStream)
#define LOG_STREAMS_SCHEDULER_SERVICE_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, STREAMS_SCHEDULER_SERVICE, logRecordStream)
#define LOG_STREAMS_SCHEDULER_SERVICE_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, STREAMS_SCHEDULER_SERVICE, logRecordStream)
#define LOG_STREAMS_SCHEDULER_SERVICE_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, STREAMS_SCHEDULER_SERVICE, logRecordStream)
#define LOG_STREAMS_SCHEDULER_SERVICE_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, STREAMS_SCHEDULER_SERVICE, logRecordStream)
#define LOG_STREAMS_SCHEDULER_SERVICE_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, STREAMS_SCHEDULER_SERVICE, logRecordStream)
#define LOG_STREAMS_SCHEDULER_SERVICE_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, STREAMS_SCHEDULER_SERVICE, logRecordStream)

// Component: STREAMS_RESOURCE_SERVICE.
#define LOG_STREAMS_RESOURCE_SERVICE_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, STREAMS_RESOURCE_SERVICE, logRecordStream)
#define LOG_STREAMS_RESOURCE_SERVICE_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, STREAMS_RESOURCE_SERVICE, logRecordStream)
#define LOG_STREAMS_RESOURCE_SERVICE_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, STREAMS_RESOURCE_SERVICE, logRecordStream)
#define LOG_STREAMS_RESOURCE_SERVICE_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, STREAMS_RESOURCE_SERVICE, logRecordStream)
#define LOG_STREAMS_RESOURCE_SERVICE_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, STREAMS_RESOURCE_SERVICE, logRecordStream)
#define LOG_STREAMS_RESOURCE_SERVICE_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, STREAMS_RESOURCE_SERVICE, logRecordStream)
#define LOG_STREAMS_RESOURCE_SERVICE_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, STREAMS_RESOURCE_SERVICE, logRecordStream)
#define LOG_STREAMS_RESOURCE_SERVICE_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, STREAMS_RESOURCE_SERVICE, logRecordStream)
#define LOG_STREAMS_RESOURCE_SERVICE_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, STREAMS_RESOURCE_SERVICE, logRecordStream)

// Component: STREAMS_CHECKPOINT_COORDINATOR.
#define LOG_STREAMS_CHECKPOINT_COORDINATOR_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, STREAMS_CHECKPOINT_COORDINATOR, logRecordStream)
#define LOG_STREAMS_CHECKPOINT_COORDINATOR_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, STREAMS_CHECKPOINT_COORDINATOR, logRecordStream)
#define LOG_STREAMS_CHECKPOINT_COORDINATOR_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, STREAMS_CHECKPOINT_COORDINATOR, logRecordStream)
#define LOG_STREAMS_CHECKPOINT_COORDINATOR_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, STREAMS_CHECKPOINT_COORDINATOR, logRecordStream)
#define LOG_STREAMS_CHECKPOINT_COORDINATOR_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, STREAMS_CHECKPOINT_COORDINATOR, logRecordStream)
#define LOG_STREAMS_CHECKPOINT_COORDINATOR_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, STREAMS_CHECKPOINT_COORDINATOR, logRecordStream)
#define LOG_STREAMS_CHECKPOINT_COORDINATOR_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, STREAMS_CHECKPOINT_COORDINATOR, logRecordStream)
#define LOG_STREAMS_CHECKPOINT_COORDINATOR_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, STREAMS_CHECKPOINT_COORDINATOR, logRecordStream)
#define LOG_STREAMS_CHECKPOINT_COORDINATOR_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, STREAMS_CHECKPOINT_COORDINATOR, logRecordStream)

// Component: STREAMS_CONTROL_PLANE_SERVICE.
#define LOG_STREAMS_CONTROL_PLANE_SERVICE_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, STREAMS_CONTROL_PLANE_SERVICE, logRecordStream)
#define LOG_STREAMS_CONTROL_PLANE_SERVICE_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, STREAMS_CONTROL_PLANE_SERVICE, logRecordStream)
#define LOG_STREAMS_CONTROL_PLANE_SERVICE_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, STREAMS_CONTROL_PLANE_SERVICE, logRecordStream)
#define LOG_STREAMS_CONTROL_PLANE_SERVICE_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, STREAMS_CONTROL_PLANE_SERVICE, logRecordStream)
#define LOG_STREAMS_CONTROL_PLANE_SERVICE_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, STREAMS_CONTROL_PLANE_SERVICE, logRecordStream)
#define LOG_STREAMS_CONTROL_PLANE_SERVICE_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, STREAMS_CONTROL_PLANE_SERVICE, logRecordStream)
#define LOG_STREAMS_CONTROL_PLANE_SERVICE_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, STREAMS_CONTROL_PLANE_SERVICE, logRecordStream)
#define LOG_STREAMS_CONTROL_PLANE_SERVICE_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, STREAMS_CONTROL_PLANE_SERVICE, logRecordStream)
#define LOG_STREAMS_CONTROL_PLANE_SERVICE_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, STREAMS_CONTROL_PLANE_SERVICE, logRecordStream)

// Component: STREAMS_GRAPH_LEADER.		
#define LOG_STREAMS_GRAPH_LEADER_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, STREAMS_GRAPH_LEADER, logRecordStream)		
#define LOG_STREAMS_GRAPH_LEADER_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, STREAMS_GRAPH_LEADER, logRecordStream)		
#define LOG_STREAMS_GRAPH_LEADER_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, STREAMS_GRAPH_LEADER, logRecordStream)		
#define LOG_STREAMS_GRAPH_LEADER_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, STREAMS_GRAPH_LEADER, logRecordStream)		
#define LOG_STREAMS_GRAPH_LEADER_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, STREAMS_GRAPH_LEADER, logRecordStream)		
#define LOG_STREAMS_GRAPH_LEADER_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, STREAMS_GRAPH_LEADER, logRecordStream)		
#define LOG_STREAMS_GRAPH_LEADER_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, STREAMS_GRAPH_LEADER, logRecordStream)		
#define LOG_STREAMS_GRAPH_LEADER_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, STREAMS_GRAPH_LEADER, logRecordStream)		
#define LOG_STREAMS_GRAPH_LEADER_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, STREAMS_GRAPH_LEADER, logRecordStream)		

// Component: YQ_CONTROL_PLANE_STORAGE.
#define LOG_YQ_CONTROL_PLANE_STORAGE_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, YQ_CONTROL_PLANE_STORAGE, logRecordStream)


#define LOG_YQ_CONTROL_PLANE_STORAGE_AS_EMERG(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, EMERG, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_AS_ALERT(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, ALERT, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_AS_CRIT(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, CRIT, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_AS_ERROR(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, ERROR, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_AS_WARN(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, WARN, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_AS_NOTICE(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, NOTICE, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_AS_INFO(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, INFO, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_AS_DEBUG(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, DEBUG, YQ_CONTROL_PLANE_STORAGE, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_STORAGE_AS_TRACE(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, TRACE, YQ_CONTROL_PLANE_STORAGE, logRecordStream)

// Component: YQ_CONTROL_PLANE_PROXY.
#define LOG_YQ_CONTROL_PLANE_PROXY_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, YQ_CONTROL_PLANE_PROXY, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_PROXY_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, YQ_CONTROL_PLANE_PROXY, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_PROXY_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, YQ_CONTROL_PLANE_PROXY, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_PROXY_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, YQ_CONTROL_PLANE_PROXY, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_PROXY_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, YQ_CONTROL_PLANE_PROXY, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_PROXY_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, YQ_CONTROL_PLANE_PROXY, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_PROXY_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, YQ_CONTROL_PLANE_PROXY, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_PROXY_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, YQ_CONTROL_PLANE_PROXY, logRecordStream)
#define LOG_YQ_CONTROL_PLANE_PROXY_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, YQ_CONTROL_PLANE_PROXY, logRecordStream)

// Component: YQ_TEST_CONNECTION.
#define LOG_YQ_TEST_CONNECTION_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, YQ_TEST_CONNECTION, logRecordStream)

#define LOG_YQ_TEST_CONNECTION_AS_EMERG(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, EMERG, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_AS_ALERT(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, ALERT, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_AS_CRIT(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, CRIT, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_AS_ERROR(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, ERROR, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_AS_WARN(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, WARN, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_AS_NOTICE(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, NOTICE, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_AS_INFO(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, INFO, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_AS_DEBUG(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, DEBUG, YQ_TEST_CONNECTION, logRecordStream)
#define LOG_YQ_TEST_CONNECTION_AS_TRACE(actorSystem, logRecordStream) LOG_STREAMS_IMPL_AS(actorSystem, TRACE, YQ_TEST_CONNECTION, logRecordStream)

// Component: YQ_AUDIT.
#define LOG_YQ_AUDIT_SERVICE_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, YQ_AUDIT, logRecordStream)
#define LOG_YQ_AUDIT_SERVICE_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, YQ_AUDIT, logRecordStream)
#define LOG_YQ_AUDIT_SERVICE_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, YQ_AUDIT, logRecordStream)
#define LOG_YQ_AUDIT_SERVICE_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, YQ_AUDIT, logRecordStream)
#define LOG_YQ_AUDIT_SERVICE_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, YQ_AUDIT, logRecordStream)
#define LOG_YQ_AUDIT_SERVICE_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, YQ_AUDIT, logRecordStream)
#define LOG_YQ_AUDIT_SERVICE_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, YQ_AUDIT, logRecordStream)
#define LOG_YQ_AUDIT_SERVICE_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, YQ_AUDIT, logRecordStream)
#define LOG_YQ_AUDIT_SERVICE_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, YQ_AUDIT, logRecordStream)

// Component: YQ_HEALTH.
#define LOG_YQ_HEALTH_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, YQ_HEALTH, logRecordStream)
#define LOG_YQ_HEALTH_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, YQ_HEALTH, logRecordStream)
#define LOG_YQ_HEALTH_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, YQ_HEALTH, logRecordStream)
#define LOG_YQ_HEALTH_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, YQ_HEALTH, logRecordStream)
#define LOG_YQ_HEALTH_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, YQ_HEALTH, logRecordStream)
#define LOG_YQ_HEALTH_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, YQ_HEALTH, logRecordStream)
#define LOG_YQ_HEALTH_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, YQ_HEALTH, logRecordStream)
#define LOG_YQ_HEALTH_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, YQ_HEALTH, logRecordStream)
#define LOG_YQ_HEALTH_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, YQ_HEALTH, logRecordStream)

// Component: FQ_CONTROL_PLANE_CONFIG.
#define LOG_FQ_CONTROL_PLANE_CONFIG_EMERG(logRecordStream) LOG_STREAMS_IMPL(EMERG, FQ_CONTROL_PLANE_CONFIG, logRecordStream)
#define LOG_FQ_CONTROL_PLANE_CONFIG_ALERT(logRecordStream) LOG_STREAMS_IMPL(ALERT, FQ_CONTROL_PLANE_CONFIG, logRecordStream)
#define LOG_FQ_CONTROL_PLANE_CONFIG_CRIT(logRecordStream) LOG_STREAMS_IMPL(CRIT, FQ_CONTROL_PLANE_CONFIG, logRecordStream)
#define LOG_FQ_CONTROL_PLANE_CONFIG_ERROR(logRecordStream) LOG_STREAMS_IMPL(ERROR, FQ_CONTROL_PLANE_CONFIG, logRecordStream)
#define LOG_FQ_CONTROL_PLANE_CONFIG_WARN(logRecordStream) LOG_STREAMS_IMPL(WARN, FQ_CONTROL_PLANE_CONFIG, logRecordStream)
#define LOG_FQ_CONTROL_PLANE_CONFIG_NOTICE(logRecordStream) LOG_STREAMS_IMPL(NOTICE, FQ_CONTROL_PLANE_CONFIG, logRecordStream)
#define LOG_FQ_CONTROL_PLANE_CONFIG_INFO(logRecordStream) LOG_STREAMS_IMPL(INFO, FQ_CONTROL_PLANE_CONFIG, logRecordStream)
#define LOG_FQ_CONTROL_PLANE_CONFIG_DEBUG(logRecordStream) LOG_STREAMS_IMPL(DEBUG, FQ_CONTROL_PLANE_CONFIG, logRecordStream)
#define LOG_FQ_CONTROL_PLANE_CONFIG_TRACE(logRecordStream) LOG_STREAMS_IMPL(TRACE, FQ_CONTROL_PLANE_CONFIG, logRecordStream)
