UNITTEST_FOR(cloud/blockstore/libs/storage/disk_agent/model)

INCLUDE(${ARCADIA_ROOT}/cloud/blockstore/tests/recipes/small.inc)

SRCS(
    device_client_ut.cpp
    device_guard_ut.cpp
    device_generator_ut.cpp
    device_scanner_ut.cpp
)

END()
