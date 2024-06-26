LIBRARY()

IF (CLANG AND NOT WITH_VALGRIND)

SRCS(
    serialization_interval.cpp
)

PEERDIR(
    contrib/libs/fmt
    contrib/libs/poco/Util
    contrib/ydb/library/yql/udfs/common/clickhouse/client
)

ADDINCL(
    contrib/ydb/library/yql/udfs/common/clickhouse/client/base
    contrib/ydb/library/yql/udfs/common/clickhouse/client/base/pcg-random
    contrib/ydb/library/yql/udfs/common/clickhouse/client/src
)

CFLAGS (
    -DARCADIA_BUILD -DUSE_PARQUET
)

GENERATE_ENUM_SERIALIZATION(serialization_interval.h)

ENDIF()

END()