PY3TEST()

INCLUDE(${ARCADIA_ROOT}/cloud/filestore/tests/recipes/medium.inc)

DEPENDS(
    cloud/filestore/tools/analytics/profile_tool
)

PEERDIR(
    cloud/filestore/tests/python/lib
    cloud/filestore/tools/testing/profile_log
)

TEST_SRCS(
    test.py
)

SET(QEMU_VIRTIO fs)

INCLUDE(${ARCADIA_ROOT}/cloud/filestore/tools/testing/fs_posix_compliance/fs_posix_compliance.inc)

INCLUDE(${ARCADIA_ROOT}/cloud/filestore/tests/recipes/service-local.inc)
INCLUDE(${ARCADIA_ROOT}/cloud/filestore/tests/recipes/vhost-local.inc)
INCLUDE(${ARCADIA_ROOT}/cloud/filestore/tests/recipes/vhost-endpoint.inc)
INCLUDE(${ARCADIA_ROOT}/cloud/storage/core/tests/recipes/qemu.inc)

END()
