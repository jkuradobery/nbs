LIBRARY()

PEERDIR(
    library/cpp/actors/core
    library/cpp/actors/interconnect
    library/cpp/blockcodecs
    library/cpp/codecs
    library/cpp/containers/intrusive_avl_tree
    library/cpp/monlib/service/pages
    ydb/core/base
    ydb/core/blobstorage/base
    ydb/core/blobstorage/groupinfo
    ydb/core/blobstorage/vdisk/common
    ydb/core/blobstorage/vdisk/hulldb/base
    ydb/core/blobstorage/vdisk/hulldb/generic
    ydb/core/blobstorage/vdisk/ingress
    ydb/core/util
)

SRCS(
    blobstorage_synclog.cpp
    blobstorage_synclog.h
    blobstorage_synclog_private_events.cpp
    blobstorage_synclog_private_events.h
    blobstorage_synclog_public_events.h
    blobstorage_synclogdata.cpp
    blobstorage_synclogdata.h
    blobstorage_synclogdsk.cpp
    blobstorage_synclogdsk.h
    blobstorage_synclogformat.cpp
    blobstorage_synclogformat.h
    blobstorage_syncloghttp.cpp
    blobstorage_syncloghttp.h
    blobstorage_synclogkeeper.cpp
    blobstorage_synclogkeeper.h
    blobstorage_synclogkeeper_committer.cpp
    blobstorage_synclogkeeper_committer.h
    blobstorage_synclogkeeper_state.cpp
    blobstorage_synclogkeeper_state.h
    blobstorage_synclogmem.cpp
    blobstorage_synclogmem.h
    blobstorage_synclogmsgimpl.cpp
    blobstorage_synclogmsgimpl.h
    blobstorage_synclogmsgreader.cpp
    blobstorage_synclogmsgreader.h
    blobstorage_synclogmsgwriter.cpp
    blobstorage_synclogmsgwriter.h
    blobstorage_synclogneighbors.cpp
    blobstorage_synclogneighbors.h
    blobstorage_synclogreader.cpp
    blobstorage_synclogreader.h
    blobstorage_synclogrecovery.cpp
    blobstorage_synclogrecovery.h
    blobstorage_synclogwriteparts.h
    defs.h
    codecs.h
)

END()

RECURSE_FOR_TESTS(
    ut
)
