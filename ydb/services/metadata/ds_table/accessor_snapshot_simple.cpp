#include "accessor_snapshot_simple.h"

namespace NKikimr::NMetadata::NProvider {

void TDSAccessorSimple::OnNewEnrichedSnapshot(NFetcher::ISnapshot::TPtr snapshot) {
    auto g = PassAwayGuard();
    OutputController->OnSnapshotConstructionResult(snapshot);
}

void TDSAccessorSimple::OnIncorrectSnapshotFromYQL(const TString& errorMessage) {
    TBase::OnIncorrectSnapshotFromYQL(errorMessage);
    auto g = PassAwayGuard();
    OutputController->OnSnapshotConstructionError(errorMessage);
}

void TDSAccessorSimple::OnSnapshotEnrichingError(const TString& errorMessage) {
    TBase::OnSnapshotEnrichingError(errorMessage);
    auto g = PassAwayGuard();
    OutputController->OnSnapshotConstructionError(errorMessage);
}

void TDSAccessorSimple::OnBootstrap() {
    Become(&TDSAccessorSimple::StateMain);
    InputController = std::make_shared<TInputController>(SelfId());
    for (auto&& i : SnapshotConstructor->GetManagers()) {
        PathesInCheck.emplace(i->GetStorageTablePath());
        Register(new TTableExistsActor(InputController, i->GetStorageTablePath(), TDuration::Seconds(2)));
    }
}

void TDSAccessorSimple::Handle(TTableExistsActor::TEvController::TEvError::TPtr& ev) {
    auto g = PassAwayGuard();
    OutputController->OnSnapshotConstructionError(ev->Get()->GetErrorMessage());
}

void TDSAccessorSimple::Handle(TTableExistsActor::TEvController::TEvResult::TPtr& ev) {
    if (!ev->Get()->IsTableExists()) {
        OutputController->OnSnapshotConstructionTableAbsent(ev->Get()->GetTablePath());
        PassAway();
        return;
    }
    Y_VERIFY(PathesInCheck.erase(ev->Get()->GetTablePath()) == 1);
    if (PathesInCheck.empty()) {
        TBase::StartSnapshotsFetching();
    }
}

}
