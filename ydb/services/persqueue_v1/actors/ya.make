LIBRARY()

PEERDIR(
    ydb/library/actors/core
    library/cpp/containers/disjoint_interval_tree
    library/cpp/string_utils/base64
    ydb/core/base
    ydb/core/grpc_services
    ydb/core/persqueue
    ydb/core/persqueue/events
    ydb/core/protos
    ydb/core/scheme
    ydb/core/tx/scheme_cache
    ydb/library/aclib
    ydb/library/persqueue/topic_parser
    ydb/public/api/protos
    ydb/public/lib/base
    ydb/services/lib/actors
    ydb/services/lib/sharding
    ydb/services/metadata
)

SRCS(
    codecs.h
    codecs.cpp
    commit_offset_actor.h
    commit_offset_actor.cpp
    events.h
    persqueue_utils.h
    persqueue_utils.cpp
    helpers.h
    helpers.cpp
    partition_actor.h
    partition_actor.cpp
    partition_id.h
    read_init_auth_actor.h
    read_init_auth_actor.cpp
    read_info_actor.h
    read_info_actor.cpp
    read_session_actor.h
    write_session_actor.h
    schema_actors.h
    schema_actors.cpp
    update_offsets_in_transaction_actor.cpp
    partition_writer.h
    partition_writer.cpp
    partition_writer_cache_actor.h
    partition_writer_cache_actor.cpp
)

END()
