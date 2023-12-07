LIBRARY()

SRCS(
    clock.cpp
)

PEERDIR(
    library/cpp/yt/assert
)

END()

RECURSE(
    benchmark
)

RECURSE_FOR_TESTS(
    unittests
)
