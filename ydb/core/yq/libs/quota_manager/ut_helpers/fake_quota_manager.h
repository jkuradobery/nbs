#pragma once
#include <ydb/core/yq/libs/quota_manager/events/events.h>

#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/core/hfunc.h>

namespace NYq {

class TQuotaServiceFakeActor: public NActors::TActor<TQuotaServiceFakeActor> {
    TQuotaMap Quotas;

public:
    TQuotaServiceFakeActor(const TQuotaMap& map = {{ QUOTA_CPU_PERCENT_LIMIT, 3200 }})
        : TActor<TQuotaServiceFakeActor>(&TQuotaServiceFakeActor::StateFunc)
        , Quotas(map)
    {
    }

    STRICT_STFUNC(StateFunc,
        hFunc(TEvQuotaService::TQuotaGetRequest, Handle);
    );

    void Handle(TEvQuotaService::TQuotaGetRequest::TPtr& ev);
};

} // namespace NYq
