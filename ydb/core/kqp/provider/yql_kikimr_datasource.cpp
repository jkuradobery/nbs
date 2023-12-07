#include "yql_kikimr_provider_impl.h"

#include <ydb/library/yql/providers/common/provider/yql_data_provider_impl.h>
#include <ydb/library/yql/providers/common/config/yql_configuration_transformer.h>

#include <ydb/library/yql/core/yql_expr_optimize.h>
#include <ydb/library/yql/core/yql_expr_type_annotation.h>

namespace NYql {
namespace {

using namespace NKikimr;
using namespace NNodes;

class TKiSourceIntentDeterminationTransformer: public TKiSourceVisitorTransformer {
public:
    TKiSourceIntentDeterminationTransformer(TIntrusivePtr<TKikimrSessionContext> sessionCtx)
        : SessionCtx(sessionCtx) {}

private:
    TStatus HandleKiRead(TKiReadBase node, TExprContext& ctx) override {
        auto cluster = node.DataSource().Cluster();
        TKikimrKey key(ctx);
        if (!key.Extract(node.TableKey().Ref())) {
            return TStatus::Error;
        }

        return HandleKey(cluster, key);
    }

    TStatus HandleRead(TExprBase node, TExprContext& ctx) override {
        auto cluster = node.Ref().Child(1)->Child(1)->Content();
        TKikimrKey key(ctx);
        if (!key.Extract(*node.Ref().Child(2))) {
            return TStatus::Error;
        }

        return HandleKey(cluster, key);
    }

    TStatus HandleLength(TExprBase node, TExprContext& ctx) override {
        Y_UNUSED(node);
        Y_UNUSED(ctx);
        return TStatus::Ok;
    }

    TStatus HandleConfigure(TExprBase node, TExprContext& ctx) override {
        Y_UNUSED(node);
        Y_UNUSED(ctx);
        return TStatus::Ok;
    }

private:
    TStatus HandleKey(const TStringBuf& cluster, const TKikimrKey& key) {
        switch (key.GetKeyType()) {
            case TKikimrKey::Type::Table:
            case TKikimrKey::Type::TableScheme: {
                auto& table = SessionCtx->Tables().GetOrAddTable(TString(cluster), SessionCtx->GetDatabase(),
                    key.GetTablePath());

                if (key.GetKeyType() == TKikimrKey::Type::TableScheme) {
                    table.RequireStats();
                }

                return TStatus::Ok;
            }

            case TKikimrKey::Type::TableList:
                return TStatus::Ok;

            case TKikimrKey::Type::Role:
                return TStatus::Ok;

            case TKikimrKey::Type::Object:
                return TStatus::Ok;
        }

        return TStatus::Error;
    }

private:
    TIntrusivePtr<TKikimrSessionContext> SessionCtx;
};

class TKiSourceLoadTableMetadataTransformer : public TGraphTransformerBase {
public:
    TKiSourceLoadTableMetadataTransformer(
        TIntrusivePtr<IKikimrGateway> gateway,
        TIntrusivePtr<TKikimrSessionContext> sessionCtx)
        : Gateway(gateway)
        , SessionCtx(sessionCtx) {}

    TStatus DoTransform(TExprNode::TPtr input, TExprNode::TPtr& output, TExprContext& ctx) final {
        output = input;

        if (ctx.Step.IsDone(TExprStep::LoadTablesMetadata)) {
            return TStatus::Ok;
        }

        size_t tablesCount = SessionCtx->Tables().GetTables().size();
        TVector<NThreading::TFuture<void>> futures;
        futures.reserve(tablesCount);

        for (auto& it : SessionCtx->Tables().GetTables()) {
            const TString& clusterName = it.first.first;
            const TString& tableName = it.first.second;
            TKikimrTableDescription& table = SessionCtx->Tables().GetTable(clusterName, tableName);

            if (table.Metadata || table.GetTableType() != ETableType::Table) {
                continue;
            }

            auto emplaceResult = LoadResults.emplace(std::make_pair(clusterName, tableName),
                std::make_shared<IKikimrGateway::TTableMetadataResult>());

            YQL_ENSURE(emplaceResult.second);
            auto queryType = SessionCtx->Query().Type;
            auto& result = emplaceResult.first->second;

            auto future = Gateway->LoadTableMetadata(clusterName, tableName,
                IKikimrGateway::TLoadTableMetadataSettings().WithTableStats(table.GetNeedsStats()));

            futures.push_back(future.Apply([result, queryType]
                (const NThreading::TFuture<IKikimrGateway::TTableMetadataResult>& future) {
                    YQL_ENSURE(!future.HasException());
                    const auto& value = future.GetValue();
                    switch (queryType) {
                        case EKikimrQueryType::Unspecified: {
                            if (value.Metadata) {
                                if (!value.Metadata->Indexes.empty()) {
                                    result->AddIssue(TIssue({}, TStringBuilder()
                                        << "Using index tables unsupported for legacy or unspecified request type"));
                                    result->SetStatus(TIssuesIds::KIKIMR_INDEX_METADATA_LOAD_FAILED);
                                    return;
                                }
                            }
                        }
                        break;
                        default:
                        break;
                    }
                    *result = value;
                }));
        }

        if (futures.empty()) {
            return TStatus::Ok;
        }

        AsyncFuture = NThreading::WaitExceptionOrAll(futures);
        return TStatus::Async;
    }

    NThreading::TFuture<void> DoGetAsyncFuture(const TExprNode& input) final {
        Y_UNUSED(input);
        return AsyncFuture;
    }

    TStatus DoApplyAsyncChanges(TExprNode::TPtr input, TExprNode::TPtr& output, TExprContext& ctx) final {
        output = input;
        YQL_ENSURE(AsyncFuture.HasValue());

        for (auto& it : LoadResults) {
            const auto& table = it.first;
            IKikimrGateway::TTableMetadataResult& res = *it.second;

            if (res.Success()) {
                res.ReportIssues(ctx.IssueManager);
                auto& tableDesc = SessionCtx->Tables().GetTable(it.first.first, it.first.second);

                YQL_ENSURE(res.Metadata);
                tableDesc.Metadata = res.Metadata;

                bool sysColumnsEnabled = SessionCtx->Config().SystemColumnsEnabled();
                YQL_ENSURE(res.Metadata->Indexes.size() == res.Metadata->SecondaryGlobalIndexMetadata.size());
                for (const auto& indexMeta : res.Metadata->SecondaryGlobalIndexMetadata) {
                    YQL_ENSURE(indexMeta);
                    auto& desc = SessionCtx->Tables().GetOrAddTable(indexMeta->Cluster, SessionCtx->GetDatabase(), indexMeta->Name);
                    desc.Metadata = indexMeta;
                    desc.Load(ctx, sysColumnsEnabled);
                }

                if (!tableDesc.Load(ctx, sysColumnsEnabled)) {
                    LoadResults.clear();
                    return TStatus::Error;
                }
            } else {
                TIssueScopeGuard issueScope(ctx.IssueManager, [input, &table, &ctx]() {
                    return MakeIntrusive<TIssue>(TIssue(ctx.GetPosition(input->Pos()), TStringBuilder()
                        << "Failed to load metadata for table: "
                        << NCommon::FullTableName(table.first, table.second)));
                });

                res.ReportIssues(ctx.IssueManager);
                LoadResults.clear();
                return TStatus::Error;
            }
        }

        LoadResults.clear();
        return TStatus::Ok;
    }

    void Rewind() final {
        LoadResults.clear();
        AsyncFuture = {};
    }

private:
    TIntrusivePtr<IKikimrGateway> Gateway;
    TIntrusivePtr<TKikimrSessionContext> SessionCtx;

    THashMap<std::pair<TString, TString>, std::shared_ptr<IKikimrGateway::TTableMetadataResult>> LoadResults;
    NThreading::TFuture<void> AsyncFuture;
};

class TKikimrConfigurationTransformer : public NCommon::TProviderConfigurationTransformer {
public:
    TKikimrConfigurationTransformer(TIntrusivePtr<TKikimrSessionContext> sessionCtx,
        const TTypeAnnotationContext& types)
        : TProviderConfigurationTransformer(sessionCtx->ConfigPtr(), types, TString(KikimrProviderName))
        , SessionCtx(sessionCtx) {}

protected:
    const THashSet<TStringBuf> AllowedScriptingPragmas = {
        "scanquery"
    };

    bool HandleAttr(TPositionHandle pos, const TString& cluster, const TString& name, const TMaybe<TString>& value,
        TExprContext& ctx) final
    {
        YQL_ENSURE(SessionCtx->Query().Type != EKikimrQueryType::Unspecified);

        bool applied = Dispatcher->Dispatch(cluster, name, value, NCommon::TSettingDispatcher::EStage::STATIC);

        if (!applied) {
            bool pragmaAllowed = false;

            switch (SessionCtx->Query().Type) {
                case EKikimrQueryType::YqlInternal:
                    pragmaAllowed = true;
                    break;

                case EKikimrQueryType::YqlScript:
                case EKikimrQueryType::YqlScriptStreaming:
                    pragmaAllowed = AllowedScriptingPragmas.contains(name);
                    break;

                default:
                    break;
            }

            if (!pragmaAllowed) {
                ctx.AddError(YqlIssue(ctx.GetPosition(pos), TIssuesIds::KIKIMR_PRAGMA_NOT_SUPPORTED, TStringBuilder()
                    << "Pragma can't be set for YDB query in current execution mode: " << name));
                return false;
            }
        }

        return true;
    }

    bool HandleAuth(TPositionHandle pos, const TString& cluster, const TString& alias, TExprContext& ctx) final {
        YQL_ENSURE(SessionCtx->Query().Type != EKikimrQueryType::Unspecified);

        if (SessionCtx->Query().Type != EKikimrQueryType::YqlInternal) {
            ctx.AddError(YqlIssue(ctx.GetPosition(pos), TIssuesIds::KIKIMR_PRAGMA_NOT_SUPPORTED, TStringBuilder()
                << "Pragma auth not supported inside Kikimr query."));
            return false;
        }

        return TProviderConfigurationTransformer::HandleAuth(pos, cluster, alias, ctx);
    }

private:
    TIntrusivePtr<TKikimrSessionContext> SessionCtx;
};

class TKikimrDataSource : public TDataProviderBase {
public:
    TKikimrDataSource(
        const NKikimr::NMiniKQL::IFunctionRegistry& functionRegistry,
        TTypeAnnotationContext& types,
        TIntrusivePtr<IKikimrGateway> gateway,
        TIntrusivePtr<TKikimrSessionContext> sessionCtx)
        : FunctionRegistry(functionRegistry)
        , Types(types)
        , Gateway(gateway)
        , SessionCtx(sessionCtx)
        , ConfigurationTransformer(new TKikimrConfigurationTransformer(sessionCtx, types))
        , IntentDeterminationTransformer(new TKiSourceIntentDeterminationTransformer(sessionCtx))
        , LoadTableMetadataTransformer(CreateKiSourceLoadTableMetadataTransformer(gateway, sessionCtx))
        , TypeAnnotationTransformer(CreateKiSourceTypeAnnotationTransformer(sessionCtx, types))
        , CallableExecutionTransformer(CreateKiSourceCallableExecutionTransformer(gateway, sessionCtx))

    {
        Y_UNUSED(FunctionRegistry);
        Y_UNUSED(Types);

        YQL_ENSURE(gateway);
        YQL_ENSURE(sessionCtx);
    }

    ~TKikimrDataSource() {}

    TStringBuf GetName() const override {
        return KikimrProviderName;
    }

    bool Initialize(TExprContext& ctx) override {
        TString defaultToken;
        if (auto credential = Types.Credentials->FindCredential(TString("default_") + KikimrProviderName)) {
            if (credential->Category != KikimrProviderName) {
                ctx.AddError(TIssue({}, TStringBuilder()
                    << "Mismatch default credential category, expected: " << KikimrProviderName
                    << ", but found: " << credential->Category));
                return false;
            }

            defaultToken = credential->Content;
        }

        if (defaultToken.empty()) {
            if (!Types.UserCredentials.OauthToken.empty()) {
                defaultToken = Types.UserCredentials.OauthToken;
            }
        }

        for (auto& cluster : Gateway->GetClusters()) {
            auto token = defaultToken;

            if (auto credential = Types.Credentials->FindCredential(TString("default_") + cluster)) {
                if (credential->Category != KikimrProviderName) {
                    ctx.AddError(TIssue({}, TStringBuilder()
                        << "Mismatch credential category, for cluster " << cluster
                        << " expected: " << KikimrProviderName
                        << ", but found: " << credential->Category));
                    return false;
                }

                token = credential->Content;
            }

            TIntrusiveConstPtr<NACLib::TUserToken> tokenPtr = new NACLib::TUserToken(token);
            if (!token.empty()) {
                Gateway->SetToken(cluster, tokenPtr);
            }
        }

        return true;
    }

    IGraphTransformer& GetConfigurationTransformer() override {
        return *ConfigurationTransformer;
    }

    IGraphTransformer& GetIntentDeterminationTransformer() override {
        return *IntentDeterminationTransformer;
    }

    IGraphTransformer& GetLoadTableMetadataTransformer() override {
        return *LoadTableMetadataTransformer;
    }

    IGraphTransformer& GetTypeAnnotationTransformer(bool instantOnly) override {
        Y_UNUSED(instantOnly);
        return *TypeAnnotationTransformer;
    }

    IGraphTransformer& GetCallableExecutionTransformer() override {
        return *CallableExecutionTransformer;
    }

    bool ValidateParameters(TExprNode& node, TExprContext& ctx, TMaybe<TString>& cluster) override {
        if (node.IsCallable(TCoDataSource::CallableName())) {
            if (node.Child(0)->Content() == YdbProviderName) {
                node.ChildRef(0) = ctx.RenameNode(*node.Child(0), KikimrProviderName);
            }

            if (node.Child(0)->Content() == KikimrProviderName) {
                if (node.Child(1)->Content().empty()) {
                    ctx.AddError(TIssue(ctx.GetPosition(node.Child(1)->Pos()), "Empty cluster name"));
                    return false;
                }

                cluster = TString(node.Child(1)->Content());
                return true;
            }
        }

        ctx.AddError(TIssue(ctx.GetPosition(node.Pos()), "Invalid Kikimr DataSource parameters."));
        return false;
    }

    bool CanParse(const TExprNode& node) override {
        if (node.IsCallable(ReadName)) {
            return node.Child(1)->Child(0)->Content() == KikimrProviderName;
        }

        if (node.IsCallable(TKiReadTable::CallableName()) ||
            node.IsCallable(TKiReadTableScheme::CallableName()) ||
            node.IsCallable(TKiReadTableList::CallableName()))
        {
            return TKiDataSource(node.ChildPtr(1)).Category() == KikimrProviderName;
        }

        YQL_ENSURE(!KikimrDataSourceFunctions().contains(node.Content()));
        return false;
    }

    bool IsPersistent(const TExprNode& node) override {
        if (node.IsCallable(ReadName)) {
            return node.Child(1)->Child(0)->Content() == KikimrProviderName;
        }

        if (node.IsCallable(TKiReadTable::CallableName())) {
            return TKiDataSource(node.ChildPtr(1)).Category() == KikimrProviderName;
        }

        return false;
    }

    bool CanPullResult(const TExprNode& node, TSyncMap& syncList, bool& canRef) override {
        Y_UNUSED(syncList);
        canRef = false;

        if (node.IsCallable(TCoRight::CallableName())) {
            const auto input = node.Child(0);
            if (input->IsCallable(TKiReadTableList::CallableName())) {
                return true;
            }

            if (input->IsCallable(TKiReadTableScheme::CallableName())) {
                return true;
            }
        }

        if (auto maybeRight = TMaybeNode<TCoNth>(&node).Tuple().Maybe<TCoRight>()) {
            if (maybeRight.Input().Maybe<TKiExecDataQuery>()) {
                return true;
            }
        }

        return false;
    }

    bool CanExecute(const TExprNode& node) override {
        if (node.IsCallable(TKiReadTableScheme::CallableName()) || node.IsCallable(TKiReadTableList::CallableName())) {
            return true;
        }

        if (auto configure = TMaybeNode<TCoConfigure>(&node)) {
            if (configure.DataSource().Maybe<TKiDataSource>()) {
                return true;
            }
        }

        return false;
    }

    TExprNode::TPtr RewriteIO(const TExprNode::TPtr& node, TExprContext& ctx) override {
        auto read = node->Child(0);
        if (!read->IsCallable(ReadName)) {
            ythrow yexception() << "Expected Read!";
        }

        TKikimrKey key(ctx);
        if (!key.Extract(*read->Child(2))) {
            return nullptr;
        }

        TString newName;
        switch (key.GetKeyType()) {
            case TKikimrKey::Type::Table:
                newName = TKiReadTable::CallableName();
                break;
            case TKikimrKey::Type::TableScheme:
                newName = TKiReadTableScheme::CallableName();
                break;
            case TKikimrKey::Type::TableList:
                newName = TKiReadTableList::CallableName();
                break;
            default:
                YQL_ENSURE(false, "Unsupported Kikimr KeyType.");
        }

        auto newRead = ctx.RenameNode(*read, newName);

        if (auto maybeRead = TMaybeNode<TKiReadTable>(newRead)) {
            auto read = maybeRead.Cast();
        }

        auto retChildren = node->ChildrenList();
        retChildren[0] = newRead;
        auto ret = ctx.ChangeChildren(*node, std::move(retChildren));
        return ret;
    }

    TExprNode::TPtr OptimizePull(const TExprNode::TPtr& source, const TFillSettings& fillSettings, TExprContext& ctx,
        IOptimizationContext& optCtx) override
    {
        auto queryType = SessionCtx->Query().Type;
        if (queryType == EKikimrQueryType::Scan) {
            return source;
        }

        if (auto execQuery = TMaybeNode<TCoNth>(source).Tuple().Maybe<TCoRight>().Input().Maybe<TKiExecDataQuery>()) {
            auto nth = TCoNth(source);
            ui32 index = ::FromString<ui32>(nth.Index());

            if (nth.Ref().GetTypeAnn()->GetKind() != ETypeAnnotationKind::List) {
                return source;
            }

            auto exec = execQuery.Cast();
            auto queryBlocks = exec.QueryBlocks();

            ui32 blockId = 0;
            ui32 startBlockIndex = 0;
            while (blockId < queryBlocks.ArgCount() && startBlockIndex + queryBlocks.Arg(blockId).Results().Size() <= index) {
                startBlockIndex += queryBlocks.Arg(blockId).Results().Size();
                ++blockId;
            }
            auto results = queryBlocks.Arg(blockId).Results();

            auto result = results.Item(index - startBlockIndex);
            ui64 rowsLimit = ::FromString<ui64>(result.RowsLimit());
            if (!rowsLimit) {
                if (!fillSettings.RowsLimitPerWrite) {
                    return source;
                }

                // NOTE: RowsLimitPerWrite in OptimizePull already incremented by one, see result provider
                // implementation for details
                rowsLimit = *fillSettings.RowsLimitPerWrite - 1;
            }

            auto newResult = Build<TKiResult>(ctx, result.Pos())
                .Value<TCoTake>()
                    .Input(result.Value())
                    .Count<TCoUint64>()
                        .Literal().Build(ToString(rowsLimit + 1))
                        .Build()
                    .Build()
                .Columns(result.Columns())
                .RowsLimit().Build(ToString(rowsLimit))
                .Done();

            auto newResults = ctx.ChangeChild(results.Ref(), index - startBlockIndex, newResult.Ptr());
            auto newQueryBlock = ctx.ChangeChild(queryBlocks.Arg(blockId).Ref(), 0, std::move(newResults));
            auto newQueryBlocks = ctx.ChangeChild(queryBlocks.Ref(), blockId, std::move(newQueryBlock));

            auto newExec = Build<TKiExecDataQuery>(ctx, exec.Pos())
                .World(exec.World())
                .DataSink(exec.DataSink())
                .QueryBlocks(newQueryBlocks)
                .Settings(exec.Settings())
                .Ast(exec.Ast())
                .Done();

            auto ret = Build<TCoNth>(ctx, nth.Pos())
                .Tuple<TCoRight>()
                    .Input(newExec)
                    .Build()
                .Index(nth.Index())
                .Done();

            optCtx.RemapNode(exec.Ref(), newExec.Ptr());
        }

        return source;
    }

    bool GetDependencies(const TExprNode& node, TExprNode::TListType& children, bool compact) override {
        Y_UNUSED(compact);
        if (CanExecute(node)) {
            children.push_back(node.ChildPtr(0));
            return true;
        }

        return false;
    }

    TString GetProviderPath(const TExprNode& node) override {
        Y_UNUSED(node);

        return TString(KikimrProviderName);
    }

private:
    const NKikimr::NMiniKQL::IFunctionRegistry& FunctionRegistry;
    TTypeAnnotationContext& Types;
    TIntrusivePtr<IKikimrGateway> Gateway;
    TIntrusivePtr<TKikimrSessionContext> SessionCtx;

    TAutoPtr<IGraphTransformer> ConfigurationTransformer;
    TAutoPtr<IGraphTransformer> IntentDeterminationTransformer;
    TAutoPtr<IGraphTransformer> LoadTableMetadataTransformer;
    TAutoPtr<IGraphTransformer> TypeAnnotationTransformer;
    TAutoPtr<IGraphTransformer> CallableExecutionTransformer;
};

} // namespace

IGraphTransformer::TStatus TKiSourceVisitorTransformer::DoTransform(TExprNode::TPtr input,
    TExprNode::TPtr& output, TExprContext& ctx)
{
    YQL_ENSURE(input->Type() == TExprNode::Callable);
    output = input;

    if (auto node = TMaybeNode<TKiReadBase>(input)) {
        return HandleKiRead(node.Cast(), ctx);
    }

    if (input->IsCallable(ReadName)) {
        return HandleRead(TExprBase(input), ctx);
    }

    if (input->IsCallable(ConfigureName)) {
        return HandleConfigure(TExprBase(input), ctx);
    }

    ctx.AddError(TIssue(ctx.GetPosition(input->Pos()), TStringBuilder() << "(Kikimr DataSource) Unsupported function: "
        << input->Content()));
    return TStatus::Error;
}

TIntrusivePtr<IDataProvider> CreateKikimrDataSource(
    const NKikimr::NMiniKQL::IFunctionRegistry& functionRegistry,
    TTypeAnnotationContext& types,
    TIntrusivePtr<IKikimrGateway> gateway,
    TIntrusivePtr<TKikimrSessionContext> sessionCtx)
{
    return new TKikimrDataSource(functionRegistry, types, gateway, sessionCtx);
}

TAutoPtr<IGraphTransformer> CreateKiSourceLoadTableMetadataTransformer(TIntrusivePtr<IKikimrGateway> gateway,
    TIntrusivePtr<TKikimrSessionContext> sessionCtx)
{
    return new TKiSourceLoadTableMetadataTransformer(gateway, sessionCtx);
}

} // namespace NYql
