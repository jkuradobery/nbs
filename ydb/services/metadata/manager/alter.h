#pragma once
#include "alter_impl.h"
#include "modification_controller.h"
#include "preparation_controller.h"
#include "restore.h"
#include "modification.h"

#include <library/cpp/actors/core/actor_bootstrapped.h>

namespace NKikimr::NMetadata::NModifications {

template <class TObject>
class TAlterActor: public TModificationActor<TObject> {
private:
    using TBase = TModificationActor<TObject>;
protected:
    virtual bool ProcessPreparedObjects(NInternal::TTableRecords&& records) const override {
        TBase::Register(new TUpdateObjectsActor<TObject>(std::move(records), TBase::UserToken,
            TBase::InternalController, TBase::SessionId, TBase::TransactionId, TBase::Context.GetUserToken()));
        return true;
    }

    virtual TString GetModificationType() const override {
        return "ALTER";
    }
public:
    using TBase::TBase;
};

template <class TObject>
class TCreateActor: public TModificationActor<TObject> {
private:
    using TBase = TModificationActor<TObject>;
protected:
    virtual bool ProcessPreparedObjects(NInternal::TTableRecords&& records) const override {
        TBase::Register(new TInsertObjectsActor<TObject>(std::move(records), TBase::UserToken,
            TBase::InternalController, TBase::SessionId, TBase::TransactionId, TBase::Context.GetUserToken()));
        return true;
    }

    virtual TString GetModificationType() const override {
        return "CREATE";
    }
public:
    using TBase::TBase;
};

template <class TObject>
class TDropActor: public TModificationActor<TObject> {
private:
    using TBase = TModificationActor<TObject>;
protected:
    using TBase::Manager;
    virtual bool BuildRestoreObjectIds() override {
        auto& first = TBase::Patches.front();
        std::vector<Ydb::Column> columns = first.SelectOwnedColumns(Manager->GetSchema().GetPKColumns());
        if (!columns.size()) {
            TBase::ExternalController->OnAlteringProblem("no pk columns in patch");
            return false;
        }
        if (columns.size() != Manager->GetSchema().GetPKColumns().size()) {
            TBase::ExternalController->OnAlteringProblem("no columns for pk detection");
            return false;
        }
        TBase::RestoreObjectIds.InitColumns(columns);
        for (auto&& i : TBase::Patches) {
            if (!TBase::RestoreObjectIds.AddRecordNativeValues(i)) {
                TBase::ExternalController->OnAlteringProblem("incorrect pk columns");
                return false;
            }
        }
        return true;
    }
    virtual TString GetModificationType() const override {
        return "DROP";
    }
public:
    using TBase::TBase;

    virtual bool ProcessPreparedObjects(NInternal::TTableRecords&& records) const override {
        TBase::Register(new TDeleteObjectsActor<TObject>(std::move(records), TBase::UserToken,
            TBase::InternalController, TBase::SessionId, TBase::TransactionId, TBase::Context.GetUserToken()));
        return true;
    }

    virtual bool PrepareRestoredObjects(std::vector<TObject>& /*objects*/) const override {
        return true;
    }

};

template <class TObject>
class TCreateCommand: public IAlterCommand {
private:
    using TBase = IAlterCommand;
protected:
    virtual void DoExecute() const override {
        typename IObjectOperationsManager<TObject>::TPtr manager = TBase::GetOperationsManagerFor<TObject>();
        Context.SetActivityType(IOperationsManager::EActivityType::Create);
        TActivationContext::AsActorContext().Register(new TCreateActor<TObject>(GetRecords(), GetController(), manager, GetContext()));
    }
public:
    using TBase::TBase;
};

template <class TObject>
class TAlterCommand: public IAlterCommand {
private:
    using TBase = IAlterCommand;
protected:
    virtual void DoExecute() const override {
        typename IObjectOperationsManager<TObject>::TPtr manager = TBase::GetOperationsManagerFor<TObject>();
        Context.SetActivityType(IOperationsManager::EActivityType::Alter);
        TActivationContext::AsActorContext().Register(new TAlterActor<TObject>(GetRecords(), GetController(), manager, GetContext()));
    }
public:
    using TBase::TBase;
};

template <class TObject>
class TDropCommand: public IAlterCommand {
private:
    using TBase = IAlterCommand;
protected:
    virtual void DoExecute() const override {
        typename IObjectOperationsManager<TObject>::TPtr manager = TBase::GetOperationsManagerFor<TObject>();
        Context.SetActivityType(IOperationsManager::EActivityType::Drop);
        TActivationContext::AsActorContext().Register(new TDropActor<TObject>(GetRecords(), GetController(), manager, GetContext()));
    }
public:
    using TBase::TBase;
};

}
