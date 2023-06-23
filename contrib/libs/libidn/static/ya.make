# Generated by devtools/yamaker.

LIBRARY()

LICENSE(
    "(GPL-2.0-or-later OR LGPL-3.0-or-later OR GPL-3.0-or-later)" AND
    "(LGPL-3.0-or-later OR GPL-2.0-or-later)" AND
    Custom-punycode AND
    FSFAP AND
    LGPL-2.0-or-later AND
    LGPL-2.1-only AND
    LGPL-2.1-or-later AND
    LGPL-3.0-only AND
    Public-Domain
)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/libiconv
)

ADDINCL(
    GLOBAL contrib/libs/libidn/include
    contrib/libs/libidn
    contrib/libs/libidn/gl
    contrib/libs/libidn/lib
    contrib/libs/libidn/lib/gl
)

NO_COMPILER_WARNINGS()

NO_RUNTIME()

CFLAGS(
    -DHAVE_CONFIG_H
    -DLIBIDN_BUILDING
    -DLOCALEDIR=\"/tmp/yamaker/libidn/out/share/locale\"
)

SRCDIR(contrib/libs/libidn)

SRCS(
    gl/basename-lgpl.c
    gl/fd-hook.c
    gl/malloca.c
    gl/stat-time.c
    lib/gl/c-ctype.c
    lib/gl/c-strcasecmp.c
    lib/gl/c-strncasecmp.c
    lib/gl/striconv.c
    lib/gl/unistr/u8-check.c
    lib/gl/unistr/u8-mbtoucr.c
    lib/gl/unistr/u8-uctomb-aux.c
    lib/gl/unistr/u8-uctomb.c
    lib/idn-free.c
    lib/idna.c
    lib/nfkc.c
    lib/pr29.c
    lib/profiles.c
    lib/punycode.c
    lib/rfc3454.c
    lib/strerror-idna.c
    lib/strerror-pr29.c
    lib/strerror-punycode.c
    lib/strerror-stringprep.c
    lib/strerror-tld.c
    lib/stringprep.c
    lib/tld.c
    lib/tlds.c
    lib/toutf8.c
    lib/version.c
)

IF (OS_LINUX)
    SRCS(
        gl/getprogname.c
    )
ELSEIF (OS_WINDOWS)
    SRCS(
        gl/getprogname.c
        lib/gl/strverscmp.c
    )
ENDIF()

END()
