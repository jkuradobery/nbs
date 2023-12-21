LIBRARY()

SRCS(
    init.cpp
)

PEERDIR(
    library/cpp/actors/core
    library/cpp/actors/http
    ydb/core/base
    ydb/core/fq/libs/actors
    ydb/core/fq/libs/audit
    ydb/core/fq/libs/checkpoint_storage
    ydb/core/fq/libs/checkpointing
    ydb/core/fq/libs/cloud_audit
    ydb/core/fq/libs/common
    ydb/core/fq/libs/compute/ydb/control_plane
    ydb/core/fq/libs/control_plane_config
    ydb/core/fq/libs/control_plane_proxy
    ydb/core/fq/libs/control_plane_storage
    ydb/core/fq/libs/events
    ydb/core/fq/libs/gateway
    ydb/core/fq/libs/health
    ydb/core/fq/libs/quota_manager
    ydb/core/fq/libs/rate_limiter/control_plane_service
    ydb/core/fq/libs/rate_limiter/quoter_service
    ydb/core/fq/libs/shared_resources
    ydb/core/fq/libs/test_connection
    ydb/core/protos
    ydb/library/folder_service
    ydb/library/folder_service/proto
    ydb/library/security
    ydb/library/yql/minikql/comp_nodes/llvm
    ydb/library/yql/utils/actor_log
    ydb/library/yql/dq/actors/compute
    ydb/library/yql/dq/comp_nodes
    ydb/library/yql/dq/transform
    ydb/library/yql/providers/common/comp_nodes
    ydb/library/yql/providers/common/metrics
    ydb/library/yql/providers/dq/actors
    ydb/library/yql/providers/dq/api/protos
    ydb/library/yql/providers/dq/provider
    ydb/library/yql/providers/dq/task_runner
    ydb/library/yql/providers/dq/worker_manager
    ydb/library/yql/providers/dq/worker_manager/interface
    ydb/library/yql/providers/generic/actors
    ydb/library/yql/providers/pq/async_io
    ydb/library/yql/providers/pq/cm_client
    ydb/library/yql/providers/pq/gateway/native
    ydb/library/yql/providers/pq/provider
    ydb/library/yql/providers/s3/actors
    ydb/library/yql/providers/s3/proto
    ydb/library/yql/providers/s3/provider
    ydb/library/yql/providers/solomon/async_io
    ydb/library/yql/providers/solomon/gateway
    ydb/library/yql/providers/solomon/proto
    ydb/library/yql/providers/solomon/provider
    ydb/library/yql/providers/ydb/actors
    ydb/library/yql/providers/ydb/comp_nodes
    ydb/library/yql/providers/ydb/provider
)

YQL_LAST_ABI_VERSION()

END()
