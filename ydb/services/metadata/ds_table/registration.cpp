#include "registration.h"

namespace NKikimr::NMetadata::NProvider {

bool TBehavioursId::operator<(const TBehavioursId& item) const {
    if (BehaviourIds.size() < item.BehaviourIds.size()) {
        return true;
    } else if (BehaviourIds.size() > item.BehaviourIds.size()) {
        return false;
    } else {
        auto itSelf = BehaviourIds.begin();
        auto itItem = item.BehaviourIds.begin();
        while (itSelf != BehaviourIds.end()) {
            if (*itSelf < *itItem) {
                return true;
            }
            ++itSelf;
            ++itItem;
        }
        return false;
    }
}

bool TBehavioursId::RemoveId(const TString& id) {
    auto it = BehaviourIds.find(id);
    if (it == BehaviourIds.end()) {
        return false;
    }
    BehaviourIds.erase(it);
    return true;
}


void TEventsCollector::Initialized(const TString& initId) {
    std::map<TBehavioursId, TEventsWaiter> movedEvents;
    for (auto it = Events.begin(); it != Events.end(); ) {
        auto m = it->first;
        if (!m.RemoveId(initId)) {
            ++it;
            continue;
        }
        if (m.IsEmpty()) {
            it->second.ResendAll(OwnerId);
        } else {
            auto itNext = Events.find(m);
            if (itNext == Events.end()) {
                movedEvents.emplace(m, std::move(it->second));
            } else {
                itNext->second.Merge(std::move(it->second));
            }
        }
        it = Events.erase(it);
    }
    for (auto&& i : movedEvents) {
        Events.emplace(i.first, std::move(i.second));
    }
}


void TRegistrationData::InitializationFinished(const TString& initId) {
    auto it = InRegistration.find(initId);
    Y_VERIFY(it != InRegistration.end());

    if (initId == NInitializer::TDBInitialization::GetTypeId()) {
        Y_VERIFY(Stage == EStage::WaitInitializerInfo);
        Stage = EStage::Active;
    }

    Registered.emplace(initId, it->second);
    InRegistration.erase(it);
    EventsWaiting->Initialized(initId);

    if (initId == NInitializer::TDBInitialization::GetTypeId()) {
        EventsWaiting->TryResendOne();
    }

}

void TRegistrationData::SetInitializationSnapshot(NFetcher::ISnapshot::TPtr s) {
    const bool notInitializedBefore = !InitializationSnapshot;
    InitializationSnapshot = dynamic_pointer_cast<NInitializer::TSnapshot>(s);
    Y_VERIFY(InitializationSnapshot);
    if (notInitializedBefore) {
        EventsWaiting->TryResendOne();
    }
}

void TRegistrationData::StartInitialization() {
    Y_VERIFY(Stage == EStage::Created);
    Stage = EStage::WaitInitializerInfo;
}

TRegistrationData::TRegistrationData() {
    InitializationFetcher = std::make_shared<NInitializer::TFetcher>();
}

void TRegistrationData::NoInitializationSnapshot() {
    InitializationSnapshot = std::make_shared<NInitializer::TSnapshot>(TInstant::Zero());
    EventsWaiting->TryResendOne();
}

}
