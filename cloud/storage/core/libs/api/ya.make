LIBRARY()

SRCS(
    authorizer.cpp
    hive_proxy.cpp
    user_stats.cpp
)

PEERDIR(
    cloud/storage/core/libs/kikimr
    ydb/library/actors/core
    ydb/core/base
)

END()
