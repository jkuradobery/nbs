# Generated by devtools/yamaker.

LIBRARY()

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/llvm12
    contrib/libs/llvm12/include
    contrib/libs/llvm12/lib/BinaryFormat
    contrib/libs/llvm12/lib/MC
    contrib/libs/llvm12/lib/Support
    contrib/libs/llvm12/lib/Target/AArch64/TargetInfo
    contrib/libs/llvm12/lib/Target/AArch64/Utils
)

ADDINCL(
    ${ARCADIA_BUILD_ROOT}/contrib/libs/llvm12/lib/Target/AArch64
    contrib/libs/llvm12/lib/Target/AArch64
    contrib/libs/llvm12/lib/Target/AArch64/MCTargetDesc
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    AArch64AsmBackend.cpp
    AArch64ELFObjectWriter.cpp
    AArch64ELFStreamer.cpp
    AArch64InstPrinter.cpp
    AArch64MCAsmInfo.cpp
    AArch64MCCodeEmitter.cpp
    AArch64MCExpr.cpp
    AArch64MCTargetDesc.cpp
    AArch64MachObjectWriter.cpp
    AArch64TargetStreamer.cpp
    AArch64WinCOFFObjectWriter.cpp
    AArch64WinCOFFStreamer.cpp
)

END()
