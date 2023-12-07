LIBRARY()

SRCS(
    context.cpp
    log.cpp
    profile.cpp
    tls_backend.cpp
)

PEERDIR(
    library/cpp/logger
    library/cpp/logger/global
    library/cpp/deprecated/atomic
)

END()

RECURSE_FOR_TESTS(
    ut
)
