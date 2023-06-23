LIBRARY()

SRCS(
    user_stats.cpp
    user_stats_actor.cpp
)

PEERDIR(
    cloud/blockstore/libs/storage/protos
    cloud/blockstore/private/api/protos

    library/cpp/actors/core

    ydb/core/base
    ydb/core/mon
)

END()
