# Generated by devtools/yamaker from nixpkgs 22.05.

LIBRARY()

LICENSE(Apache-2.0)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

VERSION(0.2.7)

ORIGINAL_SOURCE(https://github.com/awslabs/aws-c-event-stream/archive/v0.2.7.tar.gz)

PEERDIR(
    contrib/restricted/aws/aws-c-common
    contrib/restricted/aws/aws-c-io
    contrib/restricted/aws/aws-checksums
)

ADDINCL(
    GLOBAL contrib/restricted/aws/aws-c-event-stream/include
)

NO_COMPILER_WARNINGS()

NO_RUNTIME()

CFLAGS(
    -DAWS_CAL_USE_IMPORT_EXPORT
    -DAWS_CHECKSUMS_USE_IMPORT_EXPORT
    -DAWS_COMMON_USE_IMPORT_EXPORT
    -DAWS_EVENT_STREAM_USE_IMPORT_EXPORT
    -DAWS_IO_USE_IMPORT_EXPORT
    -DAWS_USE_EPOLL
    -DHAVE_SYSCONF
    -DS2N_ADX
    -DS2N_BIKE_R3_AVX2
    -DS2N_BIKE_R3_AVX512
    -DS2N_BIKE_R3_PCLMUL
    -DS2N_CLONE_SUPPORTED
    -DS2N_CPUID_AVAILABLE
    -DS2N_FALL_THROUGH_SUPPORTED
    -DS2N_FEATURES_AVAILABLE
    -DS2N_HAVE_EXECINFO
    -DS2N_KYBER512R3_AVX2_BMI2
    -DS2N_LIBCRYPTO_SUPPORTS_EVP_MD5_SHA1_HASH
    -DS2N_LIBCRYPTO_SUPPORTS_EVP_MD_CTX_SET_PKEY_CTX
    -DS2N_MADVISE_SUPPORTED
    -DS2N_SIKE_P434_R3_ASM
    -DS2N___RESTRICT__SUPPORTED
)

SRCS(
    source/event_stream.c
    source/event_stream_channel_handler.c
    source/event_stream_rpc.c
    source/event_stream_rpc_client.c
    source/event_stream_rpc_server.c
)

END()
