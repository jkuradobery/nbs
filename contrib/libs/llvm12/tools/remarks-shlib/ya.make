# Generated by devtools/yamaker.

LIBRARY()

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/llvm12
    contrib/libs/llvm12/lib/Remarks
)

ADDINCL(
    contrib/libs/llvm12/tools/remarks-shlib
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    libremarks.cpp
)

END()
