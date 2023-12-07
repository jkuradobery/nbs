IF (CLANG AND NOT WITH_VALGRIND)
LIBRARY()

SRCS(
    brotli.cpp
    bzip2.cpp
    gz.cpp
    factory.cpp
    lz4io.cpp
    zstd.cpp
    xz.cpp
)

PEERDIR(
    contrib/libs/fmt
    contrib/libs/poco/Util
    contrib/libs/brotli/dec
    contrib/libs/libbz2
    contrib/libs/lz4
    contrib/libs/lzma
    contrib/libs/zstd
)

ADDINCL(
    ydb/library/yql/udfs/common/clickhouse/client/base
    ydb/library/yql/udfs/common/clickhouse/client/base/pcg-random
    ydb/library/yql/udfs/common/clickhouse/client/src
)

YQL_LAST_ABI_VERSION()

END()
ELSE()
    LIBRARY()
    SRCS(
        factory.cpp
    )
    YQL_LAST_ABI_VERSION()
    END()
ENDIF()

