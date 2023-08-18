#include "key_provider.h"

#include "compute_client.h"
#include "kms_client.h"

#include <cloud/blockstore/libs/encryption/encryption_key.h>
#include <cloud/storage/core/libs/coroutine/executor.h>
#include <cloud/storage/core/libs/iam/iface/client.h>

#include <library/cpp/string_utils/base64/base64.h>

#include <util/string/builder.h>

namespace NCloud::NBlockStore {

using namespace NThreading;

namespace {

////////////////////////////////////////////////////////////////////////////////

class TKmsKeyProvider
    : public IKmsKeyProvider
{
private:
    const TExecutorPtr Executor;
    const NIamClient::IIamTokenClientPtr IamTokenClient;
    const IComputeClientPtr ComputeClient;
    const IKmsClientPtr KmsClient;

public:
    TKmsKeyProvider(
            TExecutorPtr executor,
            NIamClient::IIamTokenClientPtr iamTokenClient,
            IComputeClientPtr computeClient,
            IKmsClientPtr kmsClient)
        : Executor(std::move(executor))
        , IamTokenClient(std::move(iamTokenClient))
        , ComputeClient(std::move(computeClient))
        , KmsClient(std::move(kmsClient))
    {}

    TFuture<TResponse> GetKey(
        const NProto::TKmsKey& kmsKey,
        const TString& diskId)
    {
        return Executor->Execute([=] () mutable {
            return DoReadKeyFromKMS(diskId, kmsKey);
        });
    }

private:
    TResponse DoReadKeyFromKMS(
        const TString& diskId,
        const NProto::TKmsKey& kmsKey)
    {
        auto decodeResponse = SafeBase64Decode(kmsKey.GetEncryptedDEK());
        if (HasError(decodeResponse)) {
            const auto& err = decodeResponse.GetError();
            return MakeError(err.GetCode(), TStringBuilder()
                << "failed to decode dek for disk " << diskId
                << ", error: " << err.GetMessage());
        }

        auto iamFuture = IamTokenClient->GetTokenAsync();
        auto iamResponse = Executor->WaitFor(iamFuture);
        if (HasError(iamResponse)) {
            return TErrorResponse(iamResponse.GetError());
        }

        auto computeFuture = ComputeClient->CreateTokenForDEK(
            diskId,
            kmsKey.GetTaskId(),
            iamResponse.GetResult().Token);
        auto computeResponse = Executor->WaitFor(computeFuture);
        if (HasError(computeResponse)) {
            return TErrorResponse(computeResponse.GetError());
        }

        auto kmsFuture = KmsClient->Decrypt(
            kmsKey.GetKekId(),
            decodeResponse.GetResult(),
            computeResponse.GetResult());
        auto kmsResponse = Executor->WaitFor(kmsFuture);
        if (HasError(kmsResponse)) {
            return TErrorResponse(kmsResponse.GetError());
        }

        return TEncryptionKey(kmsResponse.ExtractResult());
    }

    TResultOrError<TString> SafeBase64Decode(TString encoded)
    {
        try {
            return Base64Decode(encoded);
        } catch (...) {
            // TODO: after NBS-4449
            // return MakeError(E_ARGUMENT, CurrentExceptionMessage());
            return encoded;
        }
    }
};

}   // namespace

////////////////////////////////////////////////////////////////////////////////

IKmsKeyProviderPtr CreateKmsKeyProvider(
    TExecutorPtr executor,
    NIamClient::IIamTokenClientPtr iamTokenClient,
    IComputeClientPtr computeClient,
    IKmsClientPtr kmsClient)
{
    return std::make_shared<TKmsKeyProvider>(
        std::move(executor),
        std::move(iamTokenClient),
        std::move(computeClient),
        std::move(kmsClient));
}

}   // namespace NCloud::NBlockStore
