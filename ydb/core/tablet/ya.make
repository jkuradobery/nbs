LIBRARY()

SRCS(
    bootstrapper.cpp
    defs.h
    labeled_counters_merger.cpp
    labeled_counters_merger.h
    labeled_db_counters.cpp
    labeled_db_counters.h
    node_tablet_monitor.cpp
    node_tablet_monitor.h
    node_whiteboard.cpp
    pipe_tracker.cpp
    pipe_tracker.h
    resource_broker.cpp
    resource_broker.h
    resource_broker_impl.h
    tablet_counters.cpp
    tablet_counters.h
    tablet_counters_aggregator.cpp
    tablet_counters_aggregator.h
    tablet_counters_app.cpp
    tablet_counters_app.h
    tablet_counters_protobuf.h
    tablet_exception.h
    tablet_impl.h
    tablet_list_renderer.cpp
    tablet_list_renderer.h
    tablet_metrics.cpp
    tablet_metrics.h
    tablet_monitoring_proxy.cpp
    tablet_monitoring_proxy.h
    tablet_pipe_client.cpp
    tablet_pipe_client_cache.cpp
    tablet_pipe_client_cache.h
    tablet_pipe_server.cpp
    tablet_pipecache.cpp
    tablet_req_blockbs.cpp
    tablet_req_delete.cpp
    tablet_req_findlatest.cpp
    tablet_req_rebuildhistory.cpp
    tablet_req_reset.cpp
    tablet_req_writelog.cpp
    tablet_resolver.cpp
    tablet_responsiveness_pinger.cpp
    tablet_responsiveness_pinger.h
    tablet_setup.h
    tablet_sys.cpp
    tablet_sys.h
    tablet_tracing_signals.cpp
    tablet_tracing_signals.h
    private/aggregated_counters.cpp
    private/aggregated_counters.h
    private/labeled_db_counters.cpp
    private/labeled_db_counters.h
)

PEERDIR(
    ydb/library/actors/core
    ydb/library/actors/helpers
    ydb/library/actors/protos
    ydb/library/actors/util
    library/cpp/blockcodecs
    library/cpp/deprecated/enum_codegen
    library/cpp/yson
    ydb/core/base
    ydb/core/mon
    ydb/core/mon_alloc
    ydb/core/node_whiteboard
    ydb/core/protos
    ydb/core/scheme
    ydb/core/sys_view/service
    ydb/core/tracing
    ydb/core/util
    ydb/library/persqueue/topic_parser
    ydb/library/services
)

END()

RECURSE_FOR_TESTS(
    ut
)
