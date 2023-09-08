# Generated by devtools/yamaker from nixpkgs 22.05.

LIBRARY()

VERSION(2.0.16)

ORIGINAL_SOURCE(https://github.com/numactl/numactl/archive/v2.0.16.tar.gz)

LICENSE(
    GPL-2.0-only AND
    LGPL-2.1-only AND
    Public-Domain
)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

ADDINCL(
    contrib/libs/numa
    contrib/libs/numa/internal
)

NO_COMPILER_WARNINGS()

NO_RUNTIME()

CFLAGS(
    -DHAVE_CONFIG_H
)

SRCS(
    affinity.c
    distance.c
    libnuma.c
    rtnetlink.c
    syscall.c
    sysfs.c
)

END()