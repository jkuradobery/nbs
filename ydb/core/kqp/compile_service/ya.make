LIBRARY()

SRCS(
    kqp_compile_actor.cpp
    kqp_compile_request.cpp
    kqp_compile_service.cpp
)

PEERDIR(
    ydb/core/actorlib_impl
    ydb/core/base
    ydb/core/kqp/host
)

YQL_LAST_ABI_VERSION()

END()
