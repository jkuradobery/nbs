# Generated by devtools/yamaker.

LIBRARY()

LICENSE(
    Apache-2.0 AND
    MIT AND
    Zlib
)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/curl
    contrib/libs/openssl
    contrib/restricted/aws/aws-c-common
    contrib/restricted/aws/aws-c-event-stream
)

ADDINCL(
    GLOBAL contrib/libs/aws-sdk-cpp/aws-cpp-sdk-core/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

CFLAGS(
    -DAWS_CAL_USE_IMPORT_EXPORT
    -DAWS_CHECKSUMS_USE_IMPORT_EXPORT
    -DAWS_COMMON_USE_IMPORT_EXPORT
    -DAWS_EVENT_STREAM_USE_IMPORT_EXPORT
    -DAWS_IO_USE_IMPORT_EXPORT
    -DAWS_SDK_VERSION_MAJOR=1
    -DAWS_SDK_VERSION_MINOR=8
    -DAWS_SDK_VERSION_PATCH=186
    -DAWS_USE_EPOLL
    -DCURL_HAS_H2
    -DCURL_HAS_TLS_PROXY
    -DENABLE_CURL_CLIENT
    -DENABLE_CURL_LOGGING
    -DENABLE_OPENSSL_ENCRYPTION
    -DHAS_PATHCONF
    -DHAS_UMASK
    -DS2N_ADX
    -DS2N_BIKE_R3_AVX2
    -DS2N_BIKE_R3_AVX512
    -DS2N_BIKE_R3_PCLMUL
    -DS2N_BIKE_R3_VPCLMUL
    -DS2N_CPUID_AVAILABLE
    -DS2N_FALL_THROUGH_SUPPORTED
    -DS2N_HAVE_EXECINFO
    -DS2N_KYBER512R3_AVX2_BMI2
    -DS2N_SIKE_P434_R3_ASM
    -DS2N___RESTRICT__SUPPORTED
)

SRCS(
    source/AmazonSerializableWebServiceRequest.cpp
    source/AmazonStreamingWebServiceRequest.cpp
    source/AmazonWebServiceRequest.cpp
    source/Aws.cpp
    source/Globals.cpp
    source/Region.cpp
    source/Version.cpp
    source/auth/AWSAuthSigner.cpp
    source/auth/AWSAuthSignerProvider.cpp
    source/auth/AWSCredentialsProvider.cpp
    source/auth/AWSCredentialsProviderChain.cpp
    source/auth/SSOCredentialsProvider.cpp
    source/auth/STSCredentialsProvider.cpp
    source/client/AWSClient.cpp
    source/client/AWSErrorMarshaller.cpp
    source/client/AsyncCallerContext.cpp
    source/client/ClientConfiguration.cpp
    source/client/CoreErrors.cpp
    source/client/DefaultRetryStrategy.cpp
    source/client/RetryStrategy.cpp
    source/client/SpecifiedRetryableErrorsRetryStrategy.cpp
    source/config/AWSProfileConfigLoader.cpp
    source/external/cjson/cJSON.cpp
    source/external/tinyxml2/tinyxml2.cpp
    source/http/HttpClient.cpp
    source/http/HttpClientFactory.cpp
    source/http/HttpRequest.cpp
    source/http/HttpTypes.cpp
    source/http/Scheme.cpp
    source/http/URI.cpp
    source/http/curl/CurlHandleContainer.cpp
    source/http/curl/CurlHttpClient.cpp
    source/http/standard/StandardHttpRequest.cpp
    source/http/standard/StandardHttpResponse.cpp
    source/internal/AWSHttpResourceClient.cpp
    source/monitoring/DefaultMonitoring.cpp
    source/monitoring/HttpClientMetrics.cpp
    source/monitoring/MonitoringManager.cpp
    source/utils/ARN.cpp
    source/utils/Array.cpp
    source/utils/DNS.cpp
    source/utils/DateTimeCommon.cpp
    source/utils/Directory.cpp
    source/utils/EnumParseOverflowContainer.cpp
    source/utils/FileSystemUtils.cpp
    source/utils/GetTheLights.cpp
    source/utils/HashingUtils.cpp
    source/utils/StringUtils.cpp
    source/utils/TempFile.cpp
    source/utils/UUID.cpp
    source/utils/base64/Base64.cpp
    source/utils/crypto/Cipher.cpp
    source/utils/crypto/ContentCryptoMaterial.cpp
    source/utils/crypto/ContentCryptoScheme.cpp
    source/utils/crypto/CryptoBuf.cpp
    source/utils/crypto/CryptoStream.cpp
    source/utils/crypto/EncryptionMaterials.cpp
    source/utils/crypto/KeyWrapAlgorithm.cpp
    source/utils/crypto/MD5.cpp
    source/utils/crypto/Sha1.cpp
    source/utils/crypto/Sha256.cpp
    source/utils/crypto/Sha256HMAC.cpp
    source/utils/crypto/factory/Factories.cpp
    source/utils/crypto/openssl/CryptoImpl.cpp
    source/utils/event/EventDecoderStream.cpp
    source/utils/event/EventEncoderStream.cpp
    source/utils/event/EventHeader.cpp
    source/utils/event/EventMessage.cpp
    source/utils/event/EventStreamBuf.cpp
    source/utils/event/EventStreamDecoder.cpp
    source/utils/event/EventStreamEncoder.cpp
    source/utils/event/EventStreamErrors.cpp
    source/utils/json/JsonSerializer.cpp
    source/utils/logging/AWSLogging.cpp
    source/utils/logging/ConsoleLogSystem.cpp
    source/utils/logging/DefaultLogSystem.cpp
    source/utils/logging/FormattedLogSystem.cpp
    source/utils/logging/LogLevel.cpp
    source/utils/memory/AWSMemory.cpp
    source/utils/memory/stl/SimpleStringStream.cpp
    source/utils/stream/ConcurrentStreamBuf.cpp
    source/utils/stream/PreallocatedStreamBuf.cpp
    source/utils/stream/ResponseStream.cpp
    source/utils/stream/SimpleStreamBuf.cpp
    source/utils/threading/Executor.cpp
    source/utils/threading/ReaderWriterLock.cpp
    source/utils/threading/Semaphore.cpp
    source/utils/threading/ThreadTask.cpp
    source/utils/xml/XmlSerializer.cpp
)

IF (OS_WINDOWS)
    SRCS(
        source/net/windows/Net.cpp
        source/net/windows/SimpleUDP.cpp
        source/platform/windows/Environment.cpp
        source/platform/windows/FileSystem.cpp
        source/platform/windows/OSVersionInfo.cpp
        source/platform/windows/Security.cpp
        source/platform/windows/Time.cpp
    )
ELSE()
    SRCS(
        source/net/linux-shared/Net.cpp
        source/net/linux-shared/SimpleUDP.cpp
        source/platform/linux-shared/Environment.cpp
        source/platform/linux-shared/FileSystem.cpp
        source/platform/linux-shared/OSVersionInfo.cpp
        source/platform/linux-shared/Security.cpp
        source/platform/linux-shared/Time.cpp
    )
ENDIF()

END()
