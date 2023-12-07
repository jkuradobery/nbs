#include "yql_kikimr_provider_impl.h"

#include <ydb/library/yql/utils/log/log.h>
#include <ydb/library/yql/core/common_opt/yql_co.h>
#include<ydb/library/yql/core/yql_aggregate_expander.h>
#include <ydb/library/yql/core/yql_opt_utils.h>

namespace NYql {
namespace {

using namespace NNodes;
using namespace NCommon;

TExprNode::TPtr KiTrimReadTableWorld(TExprBase node) {
    if (auto maybeRead = node.Maybe<TCoLeft>().Input().Maybe<TKiReadTable>()) {
        YQL_CLOG(INFO, ProviderKikimr) << "KiTrimReadTableWorld";
        return maybeRead.Cast().World().Ptr();
    }

    return node.Ptr();
}

TExprNode::TPtr KiEmptyCommit(TExprBase node) {
    if (!node.Maybe<TCoCommit>().World().Maybe<TCoCommit>()) {
        return node.Ptr();
    }

    auto commit = node.Cast<TCoCommit>();
    if (!commit.DataSink().Maybe<TKiDataSink>()) {
        return node.Ptr();
    }

    auto innerCommit = commit.World().Cast<TCoCommit>();
    if (!innerCommit.DataSink().Maybe<TKiDataSink>()) {
        return node.Ptr();
    }

    return innerCommit.Ptr();
}

} // namespace

TAutoPtr<IGraphTransformer> CreateKiLogicalOptProposalTransformer(TIntrusivePtr<TKikimrSessionContext> sessionCtx,
    TTypeAnnotationContext& types)
{
    Y_UNUSED(sessionCtx);
    Y_UNUSED(types);

    return CreateFunctorTransformer([](const TExprNode::TPtr& input, TExprNode::TPtr& output,
        TExprContext& ctx)
    {
        Y_UNUSED(input);
        Y_UNUSED(output);
        Y_UNUSED(ctx);

        return IGraphTransformer::TStatus::Ok;
    });
}

TAutoPtr<IGraphTransformer> CreateKiPhysicalOptProposalTransformer(TIntrusivePtr<TKikimrSessionContext> sessionCtx) {
    return CreateFunctorTransformer([sessionCtx](const TExprNode::TPtr& input, TExprNode::TPtr& output,
        TExprContext& ctx)
    {
        typedef IGraphTransformer::TStatus TStatus;

        TStatus status = OptimizeExpr(input, output, [sessionCtx](const TExprNode::TPtr& inputNode, TExprContext& ctx) {
            auto ret = inputNode;
            TExprBase node(inputNode);

            ret = KiEmptyCommit(node);
            if (ret != inputNode) {
                return ret;
            }

            if (auto maybeDatasink = node.Maybe<TCoCommit>().DataSink().Maybe<TKiDataSink>()) {
                auto cluster = TString(maybeDatasink.Cast().Cluster());

                ret = KiBuildQuery(node, ctx, sessionCtx->TablesPtr());

                if (ret != inputNode) {
                    return ret;
                }
            }

            if (sessionCtx->Config().HasDefaultCluster()) {
                auto defaultCluster = sessionCtx->Config()._DefaultCluster.Get().GetRef();
                ret = KiBuildResult(node, defaultCluster, ctx);
                if (ret != inputNode) {
                    return ret;
                }
            }

            return ret;
        }, ctx, TOptimizeExprSettings(nullptr));

        if (status.Level != IGraphTransformer::TStatus::Ok) {
            return status;
        }

        status = OptimizeExpr(input, output, [sessionCtx](const TExprNode::TPtr& inputNode, TExprContext& ctx) {
            Y_UNUSED(ctx);

            auto ret = inputNode;
            TExprBase node(inputNode);

            ret = KiTrimReadTableWorld(node);
            if (ret != inputNode) {
                return ret;
            }

            return ret;
        }, ctx, TOptimizeExprSettings(nullptr));

        return status;
    });
}

} // namespace NYql
