LIBRARY()

SRCS(
    events.cpp
)

PEERDIR(
    library/cpp/actors/core
    library/cpp/actors/interconnect
    ydb/core/yq/libs/control_plane_storage/events
    ydb/core/yq/libs/quota_manager/events
)

END()
