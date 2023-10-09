PY2_LIBRARY()

VERSION(3.2)

LICENSE(BSD-2-Clause)

NO_COMPILER_WARNINGS()

NO_LINT()

CFLAGS(
    -DUSE_SIGINFO
)

SRCS(
    faulthandler.c
    traceback.c
)

PY_REGISTER(
    faulthandler
)

RESOURCE_FILES(
    PREFIX contrib/deprecated/python/faulthandler/
    .dist-info/METADATA
    .dist-info/top_level.txt
)

END()
