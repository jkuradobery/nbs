# Generated by devtools/yamaker from nixpkgs 22.11.

LIBRARY()

LICENSE(BSL-1.0)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

VERSION(1.82.0)

ORIGINAL_SOURCE(https://github.com/boostorg/locale/archive/boost-1.82.0.tar.gz)

PEERDIR(
    contrib/libs/icu
    contrib/restricted/boost/assert
    contrib/restricted/boost/config
    contrib/restricted/boost/core
    contrib/restricted/boost/iterator
    contrib/restricted/boost/predef
    contrib/restricted/boost/thread
)

ADDINCL(
    GLOBAL contrib/restricted/boost/locale/include
    contrib/restricted/boost/locale/src
)

NO_COMPILER_WARNINGS()

NO_UTIL()

CFLAGS(
    -DBOOST_LOCALE_WITH_ICU
)

IF (DYNAMIC_BOOST)
    CFLAGS(
        GLOBAL -DBOOST_LOCALE_DYN_LINK
    )
ENDIF()

IF (OS_ANDROID)
    CFLAGS(
        -DBOOST_LOCALE_NO_POSIX_BACKEND
        -DBOOST_LOCALE_NO_WINAPI_BACKEND
    )
ELSEIF (OS_WINDOWS)
    CFLAGS(
        -DBOOST_LOCALE_NO_POSIX_BACKEND
    )
    SRCS(
        src/boost/locale/win32/collate.cpp
        src/boost/locale/win32/converter.cpp
        src/boost/locale/win32/lcid.cpp
        src/boost/locale/win32/numeric.cpp
        src/boost/locale/win32/win_backend.cpp
    )
ELSE()
    CFLAGS(
        -DBOOST_LOCALE_NO_WINAPI_BACKEND
    )
    SRCS(
        src/boost/locale/posix/codecvt.cpp
        src/boost/locale/posix/collate.cpp
        src/boost/locale/posix/converter.cpp
        src/boost/locale/posix/numeric.cpp
        src/boost/locale/posix/posix_backend.cpp
    )
ENDIF()

SRCS(
    src/boost/locale/encoding/codepage.cpp
    src/boost/locale/icu/boundary.cpp
    src/boost/locale/icu/codecvt.cpp
    src/boost/locale/icu/collator.cpp
    src/boost/locale/icu/conversion.cpp
    src/boost/locale/icu/date_time.cpp
    src/boost/locale/icu/formatter.cpp
    src/boost/locale/icu/formatters_cache.cpp
    src/boost/locale/icu/icu_backend.cpp
    src/boost/locale/icu/numeric.cpp
    src/boost/locale/icu/time_zone.cpp
    src/boost/locale/shared/date_time.cpp
    src/boost/locale/shared/format.cpp
    src/boost/locale/shared/formatting.cpp
    src/boost/locale/shared/generator.cpp
    src/boost/locale/shared/iconv_codecvt.cpp
    src/boost/locale/shared/ids.cpp
    src/boost/locale/shared/localization_backend.cpp
    src/boost/locale/shared/message.cpp
    src/boost/locale/shared/mo_lambda.cpp
    src/boost/locale/std/codecvt.cpp
    src/boost/locale/std/collate.cpp
    src/boost/locale/std/converter.cpp
    src/boost/locale/std/numeric.cpp
    src/boost/locale/std/std_backend.cpp
    src/boost/locale/util/codecvt_converter.cpp
    src/boost/locale/util/default_locale.cpp
    src/boost/locale/util/encoding.cpp
    src/boost/locale/util/gregorian.cpp
    src/boost/locale/util/info.cpp
    src/boost/locale/util/locale_data.cpp
)

END()
