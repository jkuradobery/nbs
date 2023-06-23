# Generated by devtools/yamaker from nixpkgs 22.11.

LIBRARY()

LICENSE(BSL-1.0)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

VERSION(1.82.0)

ORIGINAL_SOURCE(https://github.com/boostorg/array/archive/boost-1.82.0.tar.gz)

PEERDIR(
    contrib/restricted/boost/assert
    contrib/restricted/boost/config
    contrib/restricted/boost/core
    contrib/restricted/boost/static_assert
    contrib/restricted/boost/throw_exception
)

ADDINCL(
    GLOBAL contrib/restricted/boost/array/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

END()
