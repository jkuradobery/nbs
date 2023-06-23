LIBRARY()

SRCS(
    document_conversion.h
    http_ping.cpp
    http_ping.h
    msgbus_blobstorage_config.cpp
    msgbus_bsadm.cpp
    msgbus_http_server.h
    msgbus_http_server.cpp
    msgbus_server.cpp
    msgbus_server.h
    msgbus_server_cms.cpp
    msgbus_server_configdummy.cpp
    msgbus_server_console.cpp
    msgbus_server_db.cpp
    msgbus_server_drain_node.cpp
    msgbus_server_fill_node.cpp
    msgbus_server_get.cpp
    msgbus_server_hive_create_tablet.cpp
    msgbus_server_keyvalue.cpp
    msgbus_server_persqueue.cpp
    msgbus_server_persqueue.h
    msgbus_server_pq_metacache.h
    msgbus_server_pq_metacache.cpp
    msgbus_server_pq_metarequest.h
    msgbus_server_pq_metarequest.cpp
    msgbus_server_pq_read_session_info.cpp
    msgbus_server_resolve_node.cpp
    msgbus_server_ic_debug.cpp
    msgbus_server_load.cpp
    msgbus_server_local_enumerate_tablets.cpp
    msgbus_server_local_minikql.cpp
    msgbus_server_local_scheme_tx.cpp
    msgbus_server_login_request.cpp
    msgbus_server_node_registration.cpp
    msgbus_server_proxy.cpp
    msgbus_server_proxy.h
    msgbus_server_request.cpp
    msgbus_server_request.h
    msgbus_server_scheme_initroot.cpp
    msgbus_server_scheme_request.cpp
    msgbus_server_sqs.cpp
    msgbus_server_tablet_counters.cpp
    msgbus_server_tablet_kill.cpp
    msgbus_server_tablet_state.cpp
    msgbus_server_test_shard_request.cpp
    msgbus_server_tracer.cpp
    msgbus_server_tracer.h
    msgbus_server_tx_request.cpp
    msgbus_server_types.cpp
    msgbus_server_whoami.cpp
    msgbus_servicereq.h
    msgbus_tabletreq.h
    grpc_server.cpp
    grpc_server.h
    grpc_proxy_status.h
    grpc_proxy_status.cpp
)

PEERDIR(
    library/cpp/actors/helpers
    library/cpp/json
    library/cpp/messagebus
    library/cpp/messagebus/protobuf
    library/cpp/monlib/messagebus
    library/cpp/protobuf/json
    library/cpp/protobuf/util
    library/cpp/threading/future
    ydb/core/actorlib_impl
    ydb/core/base
    ydb/core/blobstorage/base
    ydb/core/client/metadata
    ydb/core/client/scheme_cache_lib
    ydb/core/engine
    ydb/core/engine/minikql
    ydb/core/grpc_services
    ydb/core/grpc_services/auth_processor
    ydb/core/grpc_services/base
    ydb/core/keyvalue
    ydb/core/kqp/common
    ydb/core/node_whiteboard
    ydb/core/persqueue
    ydb/core/persqueue/writer
    ydb/core/protos
    ydb/core/scheme
    ydb/core/ydb_convert
    ydb/core/ymq/actor
    ydb/library/aclib
    ydb/library/persqueue/topic_parser
    ydb/public/api/protos
    ydb/public/api/grpc
    ydb/public/api/grpc/draft
    ydb/public/lib/base
    ydb/public/lib/deprecated/client
    ydb/public/lib/deprecated/kicli
    ydb/services/persqueue_v1
    library/cpp/deprecated/atomic
)

YQL_LAST_ABI_VERSION()

END()

RECURSE_FOR_TESTS(
    ut
)
