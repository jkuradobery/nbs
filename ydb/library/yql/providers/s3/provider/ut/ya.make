UNITTEST_FOR(ydb/library/yql/providers/s3/provider)

SRCS(
    yql_s3_path_ut.cpp
)

PEERDIR(
    ydb/library/yql/public/udf/service/exception_policy
    ydb/library/yql/sql/pg_dummy
    ydb/library/yql/dq/opt
)

YQL_LAST_ABI_VERSION()

END()
