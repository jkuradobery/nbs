#pragma once

#include "type_ann_impl.h"

#include <ydb/library/yql/ast/yql_expr.h>
#include <ydb/library/yql/ast/yql_expr_types.h>

namespace NYql {
namespace NTypeAnnImpl {

    IGraphTransformer::TStatus AsScalarWrapper(const TExprNode::TPtr& input, TExprNode::TPtr& output, TContext& ctx);
    IGraphTransformer::TStatus BlockCompressWrapper(const TExprNode::TPtr& input, TExprNode::TPtr& output, TContext& ctx);
    IGraphTransformer::TStatus BlockExpandChunkedWrapper(const TExprNode::TPtr& input, TExprNode::TPtr& output, TContext& ctx);
    IGraphTransformer::TStatus BlockCoalesceWrapper(const TExprNode::TPtr& input, TExprNode::TPtr& output, TContext& ctx);
    IGraphTransformer::TStatus BlockLogicalWrapper(const TExprNode::TPtr& input, TExprNode::TPtr& output, TContext& ctx);
    IGraphTransformer::TStatus BlockIfWrapper(const TExprNode::TPtr& input, TExprNode::TPtr& output, TContext& ctx);
    IGraphTransformer::TStatus BlockFuncWrapper(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExtContext& ctx);
    IGraphTransformer::TStatus BlockBitCastWrapper(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExtContext& ctx);
    IGraphTransformer::TStatus BlockCombineAllWrapper(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExtContext& ctx);
    IGraphTransformer::TStatus BlockCombineHashedWrapper(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExtContext& ctx);
    IGraphTransformer::TStatus BlockMergeFinalizeHashedWrapper(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExtContext& ctx);

} // namespace NTypeAnnImpl
} // namespace NYql
