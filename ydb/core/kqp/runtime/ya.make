LIBRARY()

SRCS(
    kqp_channel_storage.cpp
    kqp_compute.cpp
    kqp_compute.h
    kqp_effects.cpp
    kqp_output_stream.cpp
    kqp_program_builder.cpp
    kqp_program_builder.h
    kqp_read_actor.cpp
    kqp_read_actor.h
    kqp_read_table.cpp
    kqp_read_table.h
    kqp_runtime_impl.h
    kqp_scan_data.cpp
    kqp_scan_data.h
    kqp_spilling_file.cpp
    kqp_stream_lookup_actor.cpp
    kqp_stream_lookup_actor.h
    kqp_stream_lookup_factory.cpp
    kqp_stream_lookup_factory.h
    kqp_tasks_runner.cpp
    kqp_tasks_runner.h
    kqp_transport.h
    kqp_transport.cpp
)

PEERDIR(
    contrib/libs/apache/arrow
    ydb/core/actorlib_impl
    ydb/core/base
    ydb/core/engine
    ydb/core/engine/minikql
    ydb/core/formats
    ydb/core/kqp/common
    ydb/core/protos
    ydb/core/scheme
    ydb/core/ydb_convert
    ydb/library/yql/minikql
    ydb/library/yql/minikql/comp_nodes
    ydb/library/yql/minikql/computation
    ydb/library/yql/utils
    ydb/library/yql/dq/actors/protos
    ydb/library/yql/dq/runtime
    library/cpp/threading/hot_swap
)

YQL_LAST_ABI_VERSION()

GENERATE_ENUM_SERIALIZATION(kqp_spilling.h)

END()

RECURSE_FOR_TESTS(
    ut
)
