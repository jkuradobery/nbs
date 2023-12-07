#pragma once
#include <ydb/services/metadata/request/config.h>
#include <ydb/services/metadata/request/request_actor.h>

namespace NKikimr::NMetadata::NInitializer {

class TACLModifierConstructor;

class ITableModifier {
private:
    YDB_READONLY_DEF(TString, ModificationId);
protected:
    virtual bool DoExecute(const TActorId& resultCallbackId, const NRequest::TConfig& config) const = 0;
public:
    using TPtr = std::shared_ptr<ITableModifier>;
    virtual ~ITableModifier() = default;

    ITableModifier(const TString& modificationId)
        : ModificationId(modificationId)
    {

    }

    bool Execute(const TActorId& resultCallbackId, const NRequest::TConfig& config) const {
        return DoExecute(resultCallbackId, config);
    }
};

template <class TDialogPolicy>
class TGenericTableModifier: public ITableModifier {
private:
    using TBase = ITableModifier;
    YDB_READONLY_DEF(typename TDialogPolicy::TRequest, Request);
protected:
    virtual bool DoExecute(const TActorId& resultCallbackId, const NRequest::TConfig& config) const override {
        TActivationContext::ActorSystem()->Register(new NRequest::TYDBRequest<TDialogPolicy>(Request,
            NACLib::TSystemUsers::Metadata(), resultCallbackId, config));
        return true;
    }
public:
    TGenericTableModifier(const typename TDialogPolicy::TRequest& request, const TString& modificationId)
        : TBase(modificationId)
        , Request(request)
    {

    }
};

class TACLModifierConstructor {
private:
    const TString Id;
    Ydb::Scheme::ModifyPermissionsRequest Request;
    ITableModifier::TPtr BuildModifier() const;
public:
    TACLModifierConstructor(const TString& path, const TString& id)
        : Id(id) {
        Request.set_path(path);
    }
    Ydb::Scheme::ModifyPermissionsRequest* operator->() {
        return &Request;
    }
    operator ITableModifier::TPtr() const {
        return BuildModifier();
    }
    static TACLModifierConstructor GetNoAccessModifier(const TString& path, const TString& id);
    static TACLModifierConstructor GetReadOnlyModifier(const TString& path, const TString& id);
};

class IInitializerInput {
public:
    using TPtr = std::shared_ptr<IInitializerInput>;
    virtual void OnPreparationFinished(const TVector<ITableModifier::TPtr>& modifiers) const = 0;
    virtual void OnPreparationProblem(const TString& errorMessage) const = 0;
    virtual ~IInitializerInput() = default;
};

class IInitializerOutput {
public:
    using TPtr = std::shared_ptr<IInitializerOutput>;
    virtual void OnInitializationFinished(const TString& id) const = 0;
    virtual ~IInitializerOutput() = default;
};

}
