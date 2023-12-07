#pragma once

#include <memory>

namespace NKikimr {
namespace NGRpcService {

class IRequestOpCtx;
class IFacilityProvider;

void DoImportFromS3Request(std::unique_ptr<IRequestOpCtx> p, const IFacilityProvider&);
void DoImportDataRequest(std::unique_ptr<IRequestOpCtx> p, const IFacilityProvider&);

}
}
