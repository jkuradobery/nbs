PY3_PROGRAM(ydb-dstool)

STRIP()

#
# DON'T ALLOW NEW DEPENDENCIES WITHOUT EXPLICIT APPROVE FROM  kikimr-dev@ or fomichev@
#
CHECK_DEPENDENT_DIRS(
    ALLOW_ONLY
    PEERDIRS
    arc/api/public
    build/external_resources/antlr3
    build/platform
    certs
    contrib
    library
    tools/archiver
    tools/enum_parser/enum_parser
    tools/enum_parser/enum_serialization_runtime
    tools/rescompressor
    tools/rorescompiler
    util
    ydb
)

PY_MAIN(ydb.apps.dstool.ydb-dstool)

PY_SRCS(
    ydb-dstool.py
)

PEERDIR(
    ydb/apps/dstool/lib
)

END()
