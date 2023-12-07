#include "kqp_opt_phy_effects_rules.h"
#include "kqp_opt_phy_effects_impl.h"

namespace NKikimr::NKqp::NOpt {

using namespace NYql;
using namespace NYql::NDq;
using namespace NYql::NNodes;

namespace {

TDqPhyPrecompute PrecomputeDictKeys(const TCondenseInputResult& condenseResult, TPositionHandle pos,
    TExprContext& ctx)
{
    auto computeDictKeysStage = Build<TDqStage>(ctx, pos)
        .Inputs()
            .Add(condenseResult.StageInputs)
            .Build()
        .Program()
            .Args(condenseResult.StageArgs)
            .Body<TCoMap>()
                .Input(condenseResult.Stream)
                .Lambda()
                    .Args({"dict"})
                    .Body<TCoDictKeys>()
                        .Dict("dict")
                        .Build()
                    .Build()
                .Build()
            .Build()
        .Settings().Build()
        .Done();

    return Build<TDqPhyPrecompute>(ctx, pos)
        .Connection<TDqCnValue>()
           .Output()
               .Stage(computeDictKeysStage)
               .Index().Build("0")
               .Build()
           .Build()
        .Done();
}

} // namespace

TExprBase KqpBuildDeleteIndexStages(TExprBase node, TExprContext& ctx, const TKqpOptimizeContext& kqpCtx) {
    if (!node.Maybe<TKqlDeleteRowsIndex>()) {
        return node;
    }

    auto del = node.Cast<TKqlDeleteRowsIndex>();
    const auto& table = kqpCtx.Tables->ExistingTable(kqpCtx.Cluster, del.Table().Path());
    const auto& pk = table.Metadata->KeyColumnNames;

    auto payloadSelector = Build<TCoLambda>(ctx, del.Pos())
        .Args({"stub"})
        .Body<TCoVoid>().Build()
        .Done();

    auto condenseResult = CondenseInputToDictByPk(del.Input(), table, payloadSelector, ctx);
    if (!condenseResult) {
        return node;
    }

    auto lookupKeys = PrecomputeDictKeys(*condenseResult, del.Pos(), ctx);

    const auto indexes = BuildSecondaryIndexVector(table, del.Pos(), ctx);
    YQL_ENSURE(indexes);

    auto lookupDict = PrecomputeTableLookupDict(lookupKeys, table, {}, indexes, del.Pos(), ctx);
    if (!lookupDict) {
        return node;
    }

    auto tableDelete = Build<TKqlDeleteRows>(ctx, del.Pos())
        .Table(del.Table())
        .Input(lookupKeys)
        .Done();

    TVector<TExprBase> effects;
    effects.emplace_back(tableDelete);

    for (const auto& [tableNode, indexDesc] : indexes) {
        THashSet<TStringBuf> indexTableColumns;

        for (const auto& column : indexDesc->KeyColumns) {
            YQL_ENSURE(indexTableColumns.emplace(column).second);
        }

        for (const auto& column : pk) {
            indexTableColumns.insert(column);
        }

        auto deleteIndexKeys = MakeRowsFromDict(lookupDict.Cast(), pk, indexTableColumns, del.Pos(), ctx);

        auto indexDelete = Build<TKqlDeleteRows>(ctx, del.Pos())
            .Table(tableNode)
            .Input(deleteIndexKeys)
            .Done();

        effects.emplace_back(std::move(indexDelete));
    }

    return Build<TExprList>(ctx, del.Pos())
        .Add(effects)
        .Done();
}

} // namespace NKikimr::NKqp::NOpt
