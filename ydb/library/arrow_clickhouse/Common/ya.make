LIBRARY()

PEERDIR(
    contrib/libs/apache/arrow
)

ADDINCL(
    ydb/library/arrow_clickhouse/base
    ydb/library/arrow_clickhouse
)

SRCS(
    Allocator.cpp
    PODArray.cpp
)

END()
