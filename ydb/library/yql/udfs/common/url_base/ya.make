YQL_UDF_CONTRIB(url_udf)

YQL_ABI_VERSION(
    2
    35
    0
)

SRCS(
    url_base.cpp
)

PEERDIR(
    ydb/library/yql/public/udf
    ydb/library/yql/udfs/common/url_base/lib
)

END()

RECURSE_FOR_TESTS(
    test
)

