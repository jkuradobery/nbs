UNITTEST_FOR(ydb/core/driver_lib/run)

SRCS(
    version_ut.cpp
    auto_config_initializer_ut.cpp
)

PEERDIR(ydb/library/yql/sql/pg_dummy)

END()
