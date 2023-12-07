#include "service_operation.h"
#include "operation_helpers.h"
#include "rpc_operation_request_base.h"
#include <ydb/core/grpc_services/base/base.h>
#include <ydb/core/tx/schemeshard/schemeshard_build_index.h>
#include <ydb/core/tx/schemeshard/schemeshard_export.h>
#include <ydb/core/tx/schemeshard/schemeshard_import.h>
#include <ydb/public/lib/operation_id/operation_id.h>

#include <library/cpp/actors/core/hfunc.h>

namespace NKikimr {
namespace NGRpcService {

using namespace NActors;
using namespace NSchemeShard;
using namespace NKikimrIssues;
using namespace NOperationId;
using namespace Ydb;

using TEvCancelOperationRequest = TGrpcRequestNoOperationCall<Ydb::Operations::CancelOperationRequest,
    Ydb::Operations::CancelOperationResponse>;

class TCancelOperationRPC: public TRpcOperationRequestActor<TCancelOperationRPC, TEvCancelOperationRequest> {
    TStringBuf GetLogPrefix() const override {
        switch (OperationId.GetKind()) {
        case TOperationId::EXPORT:
            return "[CancelExport]";
        case TOperationId::IMPORT:
            return "[CancelImport]";
        case TOperationId::BUILD_INDEX:
            return "[CancelIndexBuild]";
        default:
            return "[Untagged]";
        }
    }

    IEventBase* MakeRequest() override {
        switch (OperationId.GetKind()) {
        case TOperationId::EXPORT:
            return new TEvExport::TEvCancelExportRequest(TxId, DatabaseName, RawOperationId);
        case TOperationId::IMPORT:
            return new TEvImport::TEvCancelImportRequest(TxId, DatabaseName, RawOperationId);
        case TOperationId::BUILD_INDEX:
            return new TEvIndexBuilder::TEvCancelRequest(TxId, DatabaseName, RawOperationId);
        default:
            Y_FAIL("unreachable");
        }
    }

    void Handle(TEvExport::TEvCancelExportResponse::TPtr& ev) {
        const auto& record = ev->Get()->Record.GetResponse();

        LOG_D("Handle TEvExport::TEvCancelExportResponse"
            << ": record# " << record.ShortDebugString());

        Reply(record.GetStatus(), record.GetIssues());
    }

    void Handle(TEvImport::TEvCancelImportResponse::TPtr& ev) {
        const auto& record = ev->Get()->Record.GetResponse();

        LOG_D("Handle TEvImport::TEvCancelImportResponse"
            << ": record# " << record.ShortDebugString());

        Reply(record.GetStatus(), record.GetIssues());
    }

    void Handle(TEvIndexBuilder::TEvCancelResponse::TPtr& ev) {
        const auto& record = ev->Get()->Record;

        LOG_D("Handle TEvIndexBuilder::TEvCancelResponse"
            << ": record# " << record.ShortDebugString());

        Reply(record.GetStatus(), record.GetIssues());
    }

public:
    using TRpcOperationRequestActor::TRpcOperationRequestActor;

    void Bootstrap() {
        const TString& id = GetProtoRequest()->id();

        try {
            OperationId = TOperationId(id);

            switch (OperationId.GetKind()) {
            case TOperationId::EXPORT:
            case TOperationId::IMPORT:
            case TOperationId::BUILD_INDEX:
                if (!TryGetId(OperationId, RawOperationId)) {
                    return Reply(StatusIds::BAD_REQUEST, TIssuesIds::DEFAULT_ERROR, "Unable to extract operation id");
                }
                break;

            default:
                return Reply(StatusIds::UNSUPPORTED, TIssuesIds::DEFAULT_ERROR, "Unknown operation kind");
            }

            AllocateTxId();
        } catch (const yexception&) {
            return Reply(StatusIds::BAD_REQUEST, TIssuesIds::DEFAULT_ERROR, "Invalid operation id");
        }

        Become(&TCancelOperationRPC::StateWait);
    }

    STATEFN(StateWait) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvExport::TEvCancelExportResponse, Handle);
            hFunc(TEvImport::TEvCancelImportResponse, Handle);
            hFunc(TEvIndexBuilder::TEvCancelResponse, Handle);
        default:
            return StateBase(ev, TlsActivationContext->AsActorContext());
        }
    }

private:
    TOperationId OperationId;
    ui64 RawOperationId = 0;

}; // TCancelOperationRPC

void DoCancelOperationRequest(std::unique_ptr<IRequestNoOpCtx> p, const IFacilityProvider &) {
    TActivationContext::AsActorContext().Register(new TCancelOperationRPC(p.release()));
}

} // namespace NGRpcService
} // namespace NKikimr
