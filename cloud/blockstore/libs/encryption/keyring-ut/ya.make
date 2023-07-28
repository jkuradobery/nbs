PY3TEST()

INCLUDE(${ARCADIA_ROOT}/cloud/blockstore/tests/recipes/medium.inc)

TAG(
    ya:not_autocheck
    ya:manual
)

DEPENDS(
    cloud/blockstore/libs/encryption/keyring-ut/bin
)

TEST_SRCS(
    test.py
)

SET(QEMU_ENABLE_KVM False)
INCLUDE(${ARCADIA_ROOT}/cloud/storage/core/tests/recipes/qemu.inc)

END()
