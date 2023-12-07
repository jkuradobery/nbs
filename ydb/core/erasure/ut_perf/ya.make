UNITTEST_FOR(ydb/core/erasure)

FORK_SUBTESTS()
SPLIT_FACTOR(30)

TIMEOUT(60)
SIZE(SMALL)

SRCS(
    erasure_perf_test.cpp
)

END()
