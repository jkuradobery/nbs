#pragma once
#include "accessor_subscribe.h"
#include "config.h"
#include "scheme_describe.h"
#include "accessor_snapshot_simple.h"
#include "registration.h"

#include <ydb/services/metadata/service.h>
#include <ydb/services/metadata/initializer/common.h>
#include <ydb/services/metadata/initializer/events.h>
#include <ydb/services/metadata/initializer/manager.h>
#include <ydb/services/metadata/initializer/snapshot.h>
#include <ydb/services/metadata/initializer/fetcher.h>
#include <ydb/services/metadata/manager/abstract.h>

#include <library/cpp/actors/core/hfunc.h>

namespace NKikimr::NMetadata::NProvider {

class TService: public NActors::TActorBootstrapped<TService> {
private:
    using TBase = NActors::TActor<TService>;
    std::map<TString, NActors::TActorId> Accessors;
    std::shared_ptr<TRegistrationData> RegistrationData = std::make_shared<TRegistrationData>();
    const TConfig Config;

    void Handle(TEvRefreshSubscriberData::TPtr& ev);
    void Handle(TEvAskSnapshot::TPtr& ev);
    void Handle(TEvPrepareManager::TPtr& ev);
    void Handle(TEvSubscribeExternal::TPtr& ev);
    void Handle(TEvUnsubscribeExternal::TPtr& ev);
    void Handle(TEvObjectsOperation::TPtr& ev);

    void PrepareManagers(std::vector<IClassBehaviour::TPtr> managers, TAutoPtr<IEventBase> ev, const NActors::TActorId& sender);
    void Activate();

    template <class TAction>
    void ProcessEventWithFetcher(IEventHandle& ev, NFetcher::ISnapshotsFetcher::TPtr fetcher, TAction action) {
        std::vector<IClassBehaviour::TPtr> needManagers;
        for (auto&& i : fetcher->GetManagers()) {
            if (!RegistrationData->Registered.contains(i->GetTypeId())) {
                needManagers.emplace_back(i);
            }
        }
        if (needManagers.empty() || (needManagers.size() == 1 && needManagers[0]->GetTypeId() == NInitializer::TDBInitialization::GetTypeId())) {
            auto it = Accessors.find(fetcher->GetComponentId());
            if (it == Accessors.end()) {
                THolder<TExternalData> actor = MakeHolder<TExternalData>(Config, fetcher);
                it = Accessors.emplace(fetcher->GetComponentId(), Register(actor.Release())).first;
            }
            action(it->second);
        } else {
            PrepareManagers(needManagers, ev.ReleaseBase(), ev.Sender);
        }
    }

public:

    void Bootstrap(const NActors::TActorContext& ctx);

    STATEFN(StateMain) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvObjectsOperation, Handle);
            hFunc(TEvRefreshSubscriberData, Handle);
            hFunc(TEvAskSnapshot, Handle);
            hFunc(TEvPrepareManager, Handle);
            hFunc(TEvSubscribeExternal, Handle);
            hFunc(TEvUnsubscribeExternal, Handle);

            default:
                Y_VERIFY(false);
        }
    }

    TService(const TConfig& config)
        : Config(config) {
        TServiceOperator::Register(Config);
    }
};

NActors::IActor* CreateService(const TConfig& config);

}
