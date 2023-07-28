UNITTEST_FOR(cloud/blockstore/libs/storage/model)

INCLUDE(${ARCADIA_ROOT}/cloud/blockstore/tests/recipes/small.inc)

SRCS(
    composite_id_ut.cpp
    composite_task_waiter_ut.cpp
    requests_in_progress_ut.cpp
)

PEERDIR(
    cloud/blockstore/libs/storage/testlib
)

END()
