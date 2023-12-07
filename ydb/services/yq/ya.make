LIBRARY()

SRCS(
    grpc_service.cpp
)

PEERDIR(
    library/cpp/grpc/server
    library/cpp/retry
    ydb/core/grpc_services
    ydb/core/grpc_services/base
    ydb/library/protobuf_printer
    ydb/public/api/grpc
)

END()

RECURSE_FOR_TESTS(
    ut_integration
)
