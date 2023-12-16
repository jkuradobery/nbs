# Generated by devtools/yamaker from nixpkgs 22.11.

LIBRARY()

LICENSE(BSL-1.0)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

VERSION(1.84.0)

ORIGINAL_SOURCE(https://github.com/boostorg/conversion/archive/boost-1.84.0.tar.gz)

PEERDIR(
    contrib/restricted/boost/assert
    contrib/restricted/boost/config
    contrib/restricted/boost/smart_ptr
    contrib/restricted/boost/throw_exception
)

ADDINCL(
    GLOBAL contrib/restricted/boost/conversion/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

END()
