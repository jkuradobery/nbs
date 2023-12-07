LIBRARY()

SRCS(
    nodes_health_check.cpp
    rate_limiter_resources.cpp
    response_tasks.cpp
    task_get.cpp
    task_ping.cpp
    task_result_write.cpp
    utils.cpp
)

PEERDIR(
    library/cpp/actors/core
    library/cpp/lwtrace/mon
    library/cpp/monlib/service/pages
    ydb/core/base
    ydb/core/metering
    ydb/core/mon
    ydb/core/yq/libs/common
    ydb/core/yq/libs/config
    ydb/core/yq/libs/control_plane_storage/proto
    ydb/core/yq/libs/quota_manager
    ydb/core/yq/libs/quota_manager/events
    ydb/core/yq/libs/rate_limiter/events
    ydb/core/yq/libs/ydb
    ydb/library/security
    ydb/public/sdk/cpp/client/ydb_scheme
    ydb/public/sdk/cpp/client/ydb_value
    ydb/library/protobuf_printer
    ydb/library/yql/public/issue
)

YQL_LAST_ABI_VERSION()

END()
