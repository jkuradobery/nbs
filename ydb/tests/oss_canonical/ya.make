PY23_LIBRARY()

PY_SRCS(
    __init__.py
    conftest.py
    oss_canonical.py
)

PEERDIR(
    library/python/testing/yatest_common
)

END()
