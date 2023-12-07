LIBRARY()

SRCS(
    kqp_response.cpp
    kqp_session_actor.cpp
    kqp_tx.cpp
    kqp_worker_actor.cpp
    kqp_worker_common.cpp
)

PEERDIR(
    ydb/core/docapi
    ydb/core/kqp/common
)

YQL_LAST_ABI_VERSION()

END()
