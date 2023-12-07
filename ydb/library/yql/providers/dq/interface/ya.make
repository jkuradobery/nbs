LIBRARY()

SRCS(
    yql_dq_full_result_writer.cpp
    yql_dq_integration.cpp
    yql_dq_task_preprocessor.cpp
    yql_dq_task_transform.cpp
)

PEERDIR(
    contrib/libs/protobuf
    library/cpp/yson
    ydb/library/yql/ast
    ydb/library/yql/core
    ydb/library/yql/dq/tasks
)

YQL_LAST_ABI_VERSION()

END()
