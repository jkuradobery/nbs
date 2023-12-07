LIBRARY()

GENERATE_ENUM_SERIALIZATION(events.h)

GENERATE_ENUM_SERIALIZATION(event_ids.h)

PEERDIR(
    library/cpp/actors/core
    ydb/core/yq/libs/graph_params/proto
    ydb/core/yq/libs/protos
    ydb/library/yql/core/facade
    ydb/library/yql/public/issue
    ydb/public/api/protos
    ydb/public/lib/yq
    ydb/public/sdk/cpp/client/ydb_table
    ydb/library/yql/providers/common/db_id_async_resolver
    ydb/library/yql/providers/dq/provider
)

YQL_LAST_ABI_VERSION()

END()
