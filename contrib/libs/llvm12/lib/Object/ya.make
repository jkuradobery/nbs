# Generated by devtools/yamaker.

LIBRARY()

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/llvm12
    contrib/libs/llvm12/include
    contrib/libs/llvm12/lib/BinaryFormat
    contrib/libs/llvm12/lib/Bitcode/Reader
    contrib/libs/llvm12/lib/IR
    contrib/libs/llvm12/lib/MC
    contrib/libs/llvm12/lib/MC/MCParser
    contrib/libs/llvm12/lib/Support
    contrib/libs/llvm12/lib/TextAPI/MachO
)

ADDINCL(
    contrib/libs/llvm12/lib/Object
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    Archive.cpp
    ArchiveWriter.cpp
    Binary.cpp
    COFFImportFile.cpp
    COFFModuleDefinition.cpp
    COFFObjectFile.cpp
    Decompressor.cpp
    ELF.cpp
    ELFObjectFile.cpp
    Error.cpp
    IRObjectFile.cpp
    IRSymtab.cpp
    MachOObjectFile.cpp
    MachOUniversal.cpp
    MachOUniversalWriter.cpp
    Minidump.cpp
    ModuleSymbolTable.cpp
    Object.cpp
    ObjectFile.cpp
    RecordStreamer.cpp
    RelocationResolver.cpp
    SymbolSize.cpp
    SymbolicFile.cpp
    TapiFile.cpp
    TapiUniversal.cpp
    WasmObjectFile.cpp
    WindowsMachineFlag.cpp
    WindowsResource.cpp
    XCOFFObjectFile.cpp
)

END()
