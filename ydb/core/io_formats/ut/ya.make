UNITTEST_FOR(ydb/core/io_formats)

FORK_SUBTESTS()
SPLIT_FACTOR(60)
TIMEOUT(600)
SIZE(MEDIUM)

PEERDIR(
    ydb/core/io_formats

    # for NYql::NUdf alloc stuff used in binary_json
    ydb/library/yql/public/udf/service/exception_policy
    ydb/library/yql/sql/pg_dummy
)

YQL_LAST_ABI_VERSION()

SRCS(
    ut_csv.cpp
)

END()
