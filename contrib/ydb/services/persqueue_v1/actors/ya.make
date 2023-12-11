LIBRARY()

PEERDIR(
    contrib/ydb/library/actors/core
    library/cpp/containers/disjoint_interval_tree
    library/cpp/string_utils/base64
    contrib/ydb/core/util
    contrib/ydb/core/base
    contrib/ydb/core/grpc_services
    contrib/ydb/core/persqueue
    contrib/ydb/core/persqueue/events
    contrib/ydb/core/protos
    contrib/ydb/core/scheme
    contrib/ydb/core/tx/scheme_cache
    contrib/ydb/library/aclib
    contrib/ydb/library/persqueue/topic_parser
    contrib/ydb/public/api/protos
    contrib/ydb/public/lib/base
    contrib/ydb/services/lib/actors
    contrib/ydb/services/lib/sharding
    contrib/ydb/services/metadata
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
    direct_read_actor.h
    direct_read_actor.cpp
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
