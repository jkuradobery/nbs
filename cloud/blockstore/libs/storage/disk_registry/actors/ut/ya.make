UNITTEST_FOR(cloud/blockstore/libs/storage/disk_registry/actors)

INCLUDE(${ARCADIA_ROOT}/cloud/blockstore/tests/recipes/small.inc)

SRCS(
    restore_validator_actor_ut.cpp
)

PEERDIR(
    library/cpp/actors/testlib
)

END()
