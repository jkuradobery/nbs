#pragma once

#include <memory>

namespace NKikimr::NGRpcService {

class IRequestOpCtx;
class IRequestNoOpCtx;
class IFacilityProvider;

void DoExecuteQueryRequest(std::unique_ptr<IRequestNoOpCtx> p, const IFacilityProvider&);

} // namespace NKikimr::NGRpcService
