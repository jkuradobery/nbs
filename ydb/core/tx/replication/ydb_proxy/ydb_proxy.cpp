#include "ydb_proxy.h"

#include <ydb/core/protos/replication.pb.h>
#include <ydb/public/sdk/cpp/client/ydb_driver/driver.h>
#include <ydb/public/sdk/cpp/client/ydb_types/credentials/credentials.h>

#include <library/cpp/actors/core/actor.h>
#include <library/cpp/actors/core/hfunc.h>

#include <ydb/core/base/appdata.h>

#include <util/generic/hash_set.h>

#include <memory>
#include <mutex>

namespace NKikimr {
namespace NReplication {

using namespace NKikimrReplication;
using namespace NYdb;
using namespace NYdb::NScheme;
using namespace NYdb::NTable;

template <typename TDerived>
class TBaseProxyActor: public TActor<TDerived> {
    class TRequest;
    using TRequestPtr = std::shared_ptr<TRequest>;

    struct TEvPrivate {
        enum EEv {
            EvComplete = EventSpaceBegin(TKikimrEvents::ES_PRIVATE),

            EvEnd,
        };

        static_assert(EvEnd < EventSpaceEnd(TKikimrEvents::ES_PRIVATE), "expect EvEnd < EventSpaceEnd(TKikimrEvents::ES_PRIVATE)");

        struct TEvComplete: public TEventLocal<TEvComplete, EvComplete> {
            const TRequestPtr Request;

            explicit TEvComplete(const TRequestPtr& request)
                : Request(request)
            {
            }
        };

    }; // TEvPrivate

    class TRequest: public std::enable_shared_from_this<TRequest> {
        friend class TBaseProxyActor<TDerived>;

    public:
        explicit TRequest(const TActorSystem* sys, const TActorId& self, const TActorId& sender, ui64 cookie)
            : ActorSystem(sys)
            , Self(self)
            , Sender(sender)
            , Cookie(cookie)
        {
        }

        void Complete(IEventBase* ev) {
            Send(Sender, ev, Cookie);
            Send(Self, new typename TEvPrivate::TEvComplete(this->shared_from_this()));
        }

    private:
        void Send(const TActorId& recipient, IEventBase* ev, ui64 cookie = 0) {
            std::lock_guard<std::mutex> lock(RWActorSystem);

            if (ActorSystem) {
                ActorSystem->Send(new IEventHandle(recipient, Self, ev, 0, cookie));
            }
        }

        void ClearActorSystem() {
            std::lock_guard<std::mutex> lock(RWActorSystem);
            ActorSystem = nullptr;
        }

    private:
        const TActorSystem* ActorSystem;
        const TActorId Self;
        const TActorId Sender;
        const ui64 Cookie;

        std::mutex RWActorSystem;

    }; // TRequest

    struct TRequestPtrHash {
        Y_FORCE_INLINE size_t operator()(const TRequestPtr& ptr) const {
            return THash<TRequest*>()(ptr.get());
        }
    };

    void Handle(typename TEvPrivate::TEvComplete::TPtr& ev) {
        Requests.erase(ev->Get()->Request);
    }

    void PassAway() override {
        Requests.clear();
        IActor::PassAway();
    }

protected:
    using TActor<TDerived>::TActor;

    virtual ~TBaseProxyActor() {
        for (auto& request : Requests) {
            request->ClearActorSystem();
        }
    }

    std::weak_ptr<TRequest> MakeRequest(const TActorId& sender, ui64 cookie) {
        auto request = std::make_shared<TRequest>(TlsActivationContext->ActorSystem(), this->SelfId(), sender, cookie);
        Requests.emplace(request);
        return request;
    }

    template <typename TEvResponse>
    static void Complete(std::weak_ptr<TRequest> request, const typename TEvResponse::TResult& result) {
        if (auto r = request.lock()) {
            r->Complete(new TEvResponse(result));
        }
    }

    template <typename TEvResponse>
    static auto CreateCallback(std::weak_ptr<TRequest> request) {
        return [request](const typename TEvResponse::TAsyncResult& result) {
            Complete<TEvResponse>(request, result.GetValueSync());
        };
    }

    STATEFN(StateBase) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvPrivate::TEvComplete, Handle);
            sFunc(TEvents::TEvPoison, PassAway);
        }
    }

private:
    THashSet<TRequestPtr, TRequestPtrHash> Requests;

}; // TBaseProxyActor

class TYdbProxy: public TBaseProxyActor<TYdbProxy> {
    template <typename TEvResponse, typename TClient, typename... Args>
    using TFunc = typename TEvResponse::TAsyncResult(TClient::*)(Args...);

    template <typename TClient>
    TClient* EnsureClient(THolder<TClient>& client) {
        if (!client) {
            Y_VERIFY(AppData()->YdbDriver);
            client.Reset(new TClient(*AppData()->YdbDriver, Settings));
        }

        return client.Get();
    }

    template <typename TClient>
    TClient* EnsureClient() {
        if constexpr (std::is_same_v<TClient, TSchemeClient>) {
            return EnsureClient<TClient>(SchemeClient);
        } else if constexpr (std::is_same_v<TClient, TTableClient>) {
            return EnsureClient<TClient>(TableClient);
        } else {
            Y_FAIL("unreachable");
        }
    }

    template <typename TEvResponse, typename TEvRequestPtr, typename TClient, typename... Args>
    void Call(TEvRequestPtr& ev, TFunc<TEvResponse, TClient, Args...> func) {
        auto* client = EnsureClient<TClient>();
        auto request = MakeRequest(ev->Sender, ev->Cookie);
        auto args = std::move(ev->Get()->GetArgs());
        auto cb = CreateCallback<TEvResponse>(request);

        std::apply(func, std::tuple_cat(std::tie(client), std::move(args))).Subscribe(std::move(cb));
    }

    template <typename TEvResponse, typename TEvRequestPtr, typename TSession, typename... Args>
    void CallSession(TEvRequestPtr& ev, TFunc<TEvResponse, TSession, Args...> func) {
        auto* client = EnsureClient<TTableClient>();
        auto request = MakeRequest(ev->Sender, ev->Cookie);
        auto args = std::move(ev->Get()->GetArgs());
        auto cb = [request, func, args = std::move(args)](const TAsyncCreateSessionResult& result) {
            auto sessionResult = result.GetValueSync();
            if (!sessionResult.IsSuccess()) {
                return Complete<TEvYdbProxy::TEvCreateSessionResponse>(request, sessionResult);
            }

            auto session = sessionResult.GetSession();
            auto cb = CreateCallback<TEvResponse>(request);
            std::apply(func, std::tuple_cat(std::tie(session), std::move(args))).Subscribe(std::move(cb));
        };

        client->GetSession().Subscribe(std::move(cb));
    }

    void Handle(TEvYdbProxy::TEvMakeDirectoryRequest::TPtr& ev) {
        Call<TEvYdbProxy::TEvMakeDirectoryResponse>(ev, &TSchemeClient::MakeDirectory);
    }

    void Handle(TEvYdbProxy::TEvRemoveDirectoryRequest::TPtr& ev) {
        Call<TEvYdbProxy::TEvRemoveDirectoryResponse>(ev, &TSchemeClient::RemoveDirectory);
    }

    void Handle(TEvYdbProxy::TEvDescribePathRequest::TPtr& ev) {
        Call<TEvYdbProxy::TEvDescribePathResponse>(ev, &TSchemeClient::DescribePath);
    }

    void Handle(TEvYdbProxy::TEvListDirectoryRequest::TPtr& ev) {
        Call<TEvYdbProxy::TEvListDirectoryResponse>(ev, &TSchemeClient::ListDirectory);
    }

    void Handle(TEvYdbProxy::TEvModifyPermissionsRequest::TPtr& ev) {
        Call<TEvYdbProxy::TEvModifyPermissionsResponse>(ev, &TSchemeClient::ModifyPermissions);
    }

    void Handle(TEvYdbProxy::TEvCreateTableRequest::TPtr& ev) {
        CallSession<TEvYdbProxy::TEvCreateTableResponse>(ev, &TSession::CreateTable);
    }

    void Handle(TEvYdbProxy::TEvDropTableRequest::TPtr& ev) {
        CallSession<TEvYdbProxy::TEvDropTableResponse>(ev, &TSession::DropTable);
    }

    void Handle(TEvYdbProxy::TEvAlterTableRequest::TPtr& ev) {
        CallSession<TEvYdbProxy::TEvAlterTableResponse>(ev, &TSession::AlterTable);
    }

    void Handle(TEvYdbProxy::TEvCopyTableRequest::TPtr& ev) {
        CallSession<TEvYdbProxy::TEvCopyTableResponse>(ev, &TSession::CopyTable);
    }

    void Handle(TEvYdbProxy::TEvCopyTablesRequest::TPtr& ev) {
        CallSession<TEvYdbProxy::TEvCopyTablesResponse>(ev, &TSession::CopyTables);
    }

    void Handle(TEvYdbProxy::TEvRenameTablesRequest::TPtr& ev) {
        CallSession<TEvYdbProxy::TEvRenameTablesResponse>(ev, &TSession::RenameTables);
    }

    void Handle(TEvYdbProxy::TEvDescribeTableRequest::TPtr& ev) {
        CallSession<TEvYdbProxy::TEvDescribeTableResponse>(ev, &TSession::DescribeTable);
    }

    static TClientSettings MakeSettings(const TString& endpoint, const TString& database) {
        return TClientSettings()
            .DiscoveryEndpoint(endpoint)
            .DiscoveryMode(EDiscoveryMode::Async)
            .Database(database);
    }

    static TClientSettings MakeSettings(const TString& endpoint, const TString& database, const TString& token) {
        return MakeSettings(endpoint, database)
            .AuthToken(token);
    }

    static TClientSettings MakeSettings(const TString& endpoint, const TString& database, const TStaticCredentials& credentials) {
        return MakeSettings(endpoint, database)
            .CredentialsProviderFactory(CreateLoginCredentialsProviderFactory({
                .User = credentials.GetUser(),
                .Password = credentials.GetPassword(),
            }));
    }

public:
    template <typename... Args>
    explicit TYdbProxy(Args&&... args)
        : TBaseProxyActor(&TThis::StateWork)
        , Settings(MakeSettings(std::forward<Args>(args)...))
    {
    }

    STATEFN(StateWork) {
        switch (ev->GetTypeRewrite()) {
            // Scheme
            hFunc(TEvYdbProxy::TEvMakeDirectoryRequest, Handle);
            hFunc(TEvYdbProxy::TEvRemoveDirectoryRequest, Handle);
            hFunc(TEvYdbProxy::TEvDescribePathRequest, Handle);
            hFunc(TEvYdbProxy::TEvListDirectoryRequest, Handle);
            hFunc(TEvYdbProxy::TEvModifyPermissionsRequest, Handle);
            // Table
            hFunc(TEvYdbProxy::TEvCreateTableRequest, Handle);
            hFunc(TEvYdbProxy::TEvDropTableRequest, Handle);
            hFunc(TEvYdbProxy::TEvAlterTableRequest, Handle);
            hFunc(TEvYdbProxy::TEvCopyTableRequest, Handle);
            hFunc(TEvYdbProxy::TEvCopyTablesRequest, Handle);
            hFunc(TEvYdbProxy::TEvRenameTablesRequest, Handle);
            hFunc(TEvYdbProxy::TEvDescribeTableRequest, Handle);

        default:
            return StateBase(ev, TlsActivationContext->AsActorContext());
        }
    }

private:
    const TClientSettings Settings;
    THolder<TSchemeClient> SchemeClient;
    THolder<TTableClient> TableClient;

}; // TYdbProxy

IActor* CreateYdbProxy(const TString& endpoint, const TString& database) {
    return new TYdbProxy(endpoint, database);
}

IActor* CreateYdbProxy(const TString& endpoint, const TString& database, const TString& token) {
    return new TYdbProxy(endpoint, database, token);
}

IActor* CreateYdbProxy(const TString& endpoint, const TString& database, const TStaticCredentials& credentials) {
    return new TYdbProxy(endpoint, database, credentials);
}

} // NReplication
} // NKikimr
