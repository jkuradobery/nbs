# Generated by devtools/yamaker from nixpkgs 22.11.

LIBRARY()

LICENSE(BSL-1.0)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

VERSION(1.82.0)

ORIGINAL_SOURCE(https://github.com/boostorg/spirit/archive/boost-1.82.0.tar.gz)

PEERDIR(
    contrib/restricted/boost/array
    contrib/restricted/boost/assert
    contrib/restricted/boost/config
    contrib/restricted/boost/core
    contrib/restricted/boost/endian
    contrib/restricted/boost/function
    contrib/restricted/boost/function_types
    contrib/restricted/boost/fusion
    contrib/restricted/boost/integer
    contrib/restricted/boost/io
    contrib/restricted/boost/iterator
    contrib/restricted/boost/move
    contrib/restricted/boost/mpl
    contrib/restricted/boost/optional
    contrib/restricted/boost/phoenix
    contrib/restricted/boost/pool
    contrib/restricted/boost/preprocessor
    contrib/restricted/boost/proto
    contrib/restricted/boost/range
    contrib/restricted/boost/regex
    contrib/restricted/boost/smart_ptr
    contrib/restricted/boost/static_assert
    contrib/restricted/boost/thread
    contrib/restricted/boost/throw_exception
    contrib/restricted/boost/type_traits
    contrib/restricted/boost/typeof
    contrib/restricted/boost/unordered
    contrib/restricted/boost/utility
    contrib/restricted/boost/variant
)

ADDINCL(
    GLOBAL contrib/restricted/boost/spirit/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

END()
