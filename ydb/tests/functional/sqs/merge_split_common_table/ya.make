PY3_LIBRARY()


PY_SRCS(
    __init__.py
    test.py
)

PEERDIR(
    ydb/tests/library
    ydb/tests/library/sqs
    contrib/python/xmltodict
    contrib/python/boto3
    contrib/python/botocore
)

END()
