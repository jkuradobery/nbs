PY3_PROGRAM(local-null-recipe)

PY_SRCS(__main__.py)

PEERDIR(
    cloud/blockstore/config
    cloud/blockstore/tests/python/lib

    library/python/testing/recipe
    library/python/testing/yatest_common

    ydb/tests/library
)

DATA(
    arcadia/cloud/blockstore/tests/certs/server.crt
    arcadia/cloud/blockstore/tests/certs/server.key
)

END()
