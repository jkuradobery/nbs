#pragma once

namespace NKikimr {
namespace NGRpcService {

class IRequestProxyCtx;

void AuditLog(const IRequestProxyCtx* reqCtx, const TString& database,
              const TString& subject, const TActorContext& ctx);

}
}
