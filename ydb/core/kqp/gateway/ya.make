LIBRARY()

SRCS(
    kqp_ic_gateway.cpp
    kqp_metadata_loader.cpp
    kqp_query_data.cpp
)

PEERDIR(
    library/cpp/actors/core
    ydb/core/actorlib_impl
    ydb/core/base
    ydb/core/kqp/common
    ydb/library/yql/providers/result/expr_nodes
)

YQL_LAST_ABI_VERSION()

END()
