#include "yql_expr_constraint.h"
#include "yql_callable_transform.h"
#include "yql_opt_utils.h"

#include <ydb/library/yql/core/expr_nodes/yql_expr_nodes.h>
#include <ydb/library/yql/core/yql_type_helpers.h>
#include <ydb/library/yql/core/yql_join.h>
#include <ydb/library/yql/utils/log/profile.h>

#include <util/generic/scope.h>
#include <util/generic/maybe.h>
#include <util/generic/hash.h>
#include <util/generic/utility.h>
#include <util/generic/algorithm.h>
#include <util/string/builder.h>
#include <util/string/type.h>


namespace NYql {

using namespace NNodes;

namespace {

template <size_t FromChild, class... Other>
struct TApplyConstraintFromInput;

template <size_t FromChild>
struct TApplyConstraintFromInput<FromChild> {
    static void Do(const TExprNode::TPtr&) {
    }
};

template <size_t FromChild, class TConstraint, class... Other>
struct TApplyConstraintFromInput<FromChild, TConstraint, Other...> {
    static void Do(const TExprNode::TPtr& input) {
        if (auto c = input->Child(FromChild)->GetConstraint<TConstraint>()) {
            input->AddConstraint(c);
        }
        TApplyConstraintFromInput<FromChild, Other...>::Do(input);
    }
};

template <class TConstraint>
const TConstraint* MakeCommonConstraint(const TExprNode::TPtr& input, size_t from, TExprContext& ctx) {
    TVector<const TConstraintSet*> constraints;
    for (size_t i = from; i < input->ChildrenSize(); ++i) {
        constraints.push_back(&input->Child(i)->GetConstraintSet());
    }
    return TConstraint::MakeCommon(constraints, ctx);
}

template <class... Other>
struct TApplyCommonConstraint;

template <class TConstraint>
struct TApplyCommonConstraint<TConstraint> {
    static void Do(const TExprNode::TPtr& input, const std::vector<const TConstraintSet*>& constraints, TExprContext& ctx) {
        if (auto c = TConstraint::MakeCommon(constraints, ctx)) {
            input->AddConstraint(c);
        }
    }
};

template <class TConstraint, class... Other>
struct TApplyCommonConstraint<TConstraint, Other...> {
    static void Do(const TExprNode::TPtr& input, const std::vector<const TConstraintSet*>& constraints, TExprContext& ctx) {
        if (auto c = TConstraint::MakeCommon(constraints, ctx)) {
            input->AddConstraint(c);
        }
        TApplyCommonConstraint<Other...>::Do(input, constraints, ctx);
    }
};

class TCallableConstraintTransformer : public TCallableTransformerBase<TCallableConstraintTransformer> {
    using THandler = TStatus(TCallableConstraintTransformer::*)(const TExprNode::TPtr&, TExprNode::TPtr&, TExprContext&) const;

public:
    TCallableConstraintTransformer(TTypeAnnotationContext& types, bool instantOnly, bool subGraph)
        : TCallableTransformerBase<TCallableConstraintTransformer>(types, instantOnly)
        , SubGraph(subGraph)
    {
        Functions["Unordered"] = &TCallableConstraintTransformer::FromFirst<TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>;
        Functions["UnorderedSubquery"] = &TCallableConstraintTransformer::FromFirst<TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>;
        Functions["Sort"] = &TCallableConstraintTransformer::SortWrap;
        Functions["AssumeSorted"] = &TCallableConstraintTransformer::AssumeSortedWrap;
        Functions["AssumeUnique"] = &TCallableConstraintTransformer::AssumeUniqueWrap;
        Functions["AssumeColumnOrder"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["AssumeAllMembersNullableAtOnce"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["Top"] = &TCallableConstraintTransformer::TopWrap<false>;
        Functions["TopSort"] = &TCallableConstraintTransformer::TopWrap<true>;
        Functions["TakeWhile"] = &TCallableConstraintTransformer::FilterWrap<true>;
        Functions["SkipWhile"] = &TCallableConstraintTransformer::FilterWrap<true>;
        Functions["TakeWhileInclusive"] = &TCallableConstraintTransformer::FilterWrap<true>;
        Functions["SkipWhileInclusive"] = &TCallableConstraintTransformer::FilterWrap<true>;
        Functions["WideTakeWhile"] = &TCallableConstraintTransformer::FilterWrap<true>;
        Functions["WideSkipWhile"] = &TCallableConstraintTransformer::FilterWrap<true>;
        Functions["WideTakeWhileInclusive"] = &TCallableConstraintTransformer::FilterWrap<true>;
        Functions["WideSkipWhileInclusive"] = &TCallableConstraintTransformer::FilterWrap<true>;
        Functions["Iterator"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["ForwardList"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["LazyList"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["ToFlow"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["FromFlow"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["ToStream"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["ToSequence"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["Collect"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["FilterNullMembers"] = &TCallableConstraintTransformer::FromFirst<TSortedConstraintNode, TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode>;
        Functions["SkipNullMembers"] = &TCallableConstraintTransformer::FromFirst<TSortedConstraintNode, TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode>;
        Functions["FilterNullElements"] = &TCallableConstraintTransformer::FromFirst<TPassthroughConstraintNode, TEmptyConstraintNode, TVarIndexConstraintNode>;
        Functions["SkipNullElements"] = &TCallableConstraintTransformer::FromFirst<TPassthroughConstraintNode, TEmptyConstraintNode, TVarIndexConstraintNode>;
        Functions["Right!"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["Cons!"] = &TCallableConstraintTransformer::CopyAllFrom<1>;
        Functions["ExtractMembers"] = &TCallableConstraintTransformer::ExtractMembersWrap;
        Functions["RemoveSystemMembers"] = &TCallableConstraintTransformer::RemovePrefixMembersWrap;
        Functions["RemovePrefixMembers"] = &TCallableConstraintTransformer::RemovePrefixMembersWrap;
        Functions["SelectMembers"] = &TCallableConstraintTransformer::SelectMembersWrap;
        Functions["FilterMembers"] = &TCallableConstraintTransformer::SelectMembersWrap;
        Functions["CastStruct"] = &TCallableConstraintTransformer::SelectMembersWrap;
        Functions["SafeCast"] = &TCallableConstraintTransformer::SelectMembersWrap<true>;
        Functions["StrictCast"] = &TCallableConstraintTransformer::SelectMembersWrap<true>;
        Functions["DivePrefixMembers"] = &TCallableConstraintTransformer::DivePrefixMembersWrap;
        Functions["OrderedFilter"] = &TCallableConstraintTransformer::FilterWrap<true>;
        Functions["Filter"] = &TCallableConstraintTransformer::FilterWrap<false>;
        Functions["WideFilter"] = &TCallableConstraintTransformer::FilterWrap<true>;
        Functions["OrderedMap"] = &TCallableConstraintTransformer::MapWrap<true, false>;
        Functions["Map"] = &TCallableConstraintTransformer::MapWrap<false, false>;
        Functions["MapNext"] = &TCallableConstraintTransformer::MapWrap<true, false>;
        Functions["OrderedFlatMap"] = &TCallableConstraintTransformer::MapWrap<true, true>;
        Functions["FlatMap"] = &TCallableConstraintTransformer::MapWrap<false, true>;
        Functions["OrderedMultiMap"] = &TCallableConstraintTransformer::MapWrap<true, false>;
        Functions["MultiMap"] = &TCallableConstraintTransformer::MapWrap<false, false>;
        Functions["ExpandMap"] = &TCallableConstraintTransformer::MapWrap<true, false, false, true>;
        Functions["WideMap"] = &TCallableConstraintTransformer::MapWrap<true, false, true, true>;
        Functions["NarrowMap"] = &TCallableConstraintTransformer::MapWrap<true, false, true, false>;
        Functions["NarrowFlatMap"] = &TCallableConstraintTransformer::MapWrap<true, true, true, false>;
        Functions["NarrowMultiMap"] = &TCallableConstraintTransformer::MapWrap<true, false, true, false>;
        Functions["OrderedFlatMapToEquiJoin"] = &TCallableConstraintTransformer::MapWrap<true, true>;
        Functions["FlatMapToEquiJoin"] = &TCallableConstraintTransformer::MapWrap<false, true>;
        Functions["OrderedLMap"] = &TCallableConstraintTransformer::LMapWrap<true>;
        Functions["LMap"] = &TCallableConstraintTransformer::LMapWrap<false>;
        Functions["Extract"] = &TCallableConstraintTransformer::FromFirst<TEmptyConstraintNode>;
        Functions["OrderedExtract"] = &TCallableConstraintTransformer::FromFirst<TEmptyConstraintNode>;
        Functions["OrderedExtend"] = &TCallableConstraintTransformer::OrderedExtendWrap;
        Functions["Extend"] = &TCallableConstraintTransformer::ExtendWrap<false>;
        Functions["UnionAll"] = &TCallableConstraintTransformer::ExtendWrap<false>;
        Functions["Merge"] = &TCallableConstraintTransformer::MergeWrap<false>;
        Functions["UnionMerge"] = &TCallableConstraintTransformer::MergeWrap<true>;
        Functions["Skip"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["Take"] = &TCallableConstraintTransformer::TakeWrap;
        Functions["Limit"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["Member"] = &TCallableConstraintTransformer::MemberWrap;
        Functions["AsStruct"] = &TCallableConstraintTransformer::AsStructWrap;
        Functions["Just"] = &TCallableConstraintTransformer::FromFirst<TPassthroughConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>;
        Functions["Unwrap"] = &TCallableConstraintTransformer::FromFirst<TPassthroughConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode, TEmptyConstraintNode>;
        Functions["ToList"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["ToOptional"] = &TCallableConstraintTransformer::FromFirst<TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>;
        Functions["Head"] = &TCallableConstraintTransformer::FromFirst<TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>;
        Functions["Last"] = &TCallableConstraintTransformer::FromFirst<TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>;
        Functions["Reverse"] = &TCallableConstraintTransformer::FromFirst<TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>;
        Functions["Replicate"] = &TCallableConstraintTransformer::FromFirst<TPassthroughConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>;
        Functions["AddMember"] = &TCallableConstraintTransformer::AddMemberWrap;
        Functions["RemoveMember"] = &TCallableConstraintTransformer::RemoveMemberWrap;
        Functions["ForceRemoveMember"] = &TCallableConstraintTransformer::RemoveMemberWrap;
        Functions["ReplaceMember"] = &TCallableConstraintTransformer::ReplaceMemberWrap;
        Functions["AsList"] = &TCallableConstraintTransformer::ExtendWrap<true>;
        Functions["OptionalIf"] = &TCallableConstraintTransformer::FromSecond<TPassthroughConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>;
        Functions["ListIf"] = &TCallableConstraintTransformer::CopyAllFrom<1>;
        Functions["FlatListIf"] = &TCallableConstraintTransformer::CopyAllFrom<1>;
        Functions["FlatOptionalIf"] = &TCallableConstraintTransformer::CopyAllFrom<1>;
        Functions["EmptyIterator"] = &TCallableConstraintTransformer::FromEmpty;
        Functions["List"] = &TCallableConstraintTransformer::ListWrap;
        Functions["Dict"] = &TCallableConstraintTransformer::DictWrap;
        Functions["EmptyList"] = &TCallableConstraintTransformer::FromEmpty;
        Functions["EmptyDict"] = &TCallableConstraintTransformer::FromEmpty;
        Functions["DictItems"] = &TCallableConstraintTransformer::FromFirst<TEmptyConstraintNode>;
        Functions["DictKeys"] = &TCallableConstraintTransformer::FromFirst<TEmptyConstraintNode>;
        Functions["DictPayloads"] = &TCallableConstraintTransformer::FromFirst<TEmptyConstraintNode>;
        Functions["DictFromKeys"] = &TCallableConstraintTransformer::DictFromKeysWrap;
        Functions["If"] = &TCallableConstraintTransformer::IfWrap;
        Functions["Nothing"] = &TCallableConstraintTransformer::FromEmpty;
        Functions["IfPresent"] = &TCallableConstraintTransformer::IfPresentWrap;
        Functions["Coalesce"] = &TCallableConstraintTransformer::CommonFromChildren<0, TSortedConstraintNode, TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>;
        Functions["CombineByKey"] = &TCallableConstraintTransformer::FromFinalLambda<TCoCombineByKey::idx_FinishHandlerLambda>;
        Functions["FinalizeByKey"] = &TCallableConstraintTransformer::FromFinalLambda<TCoFinalizeByKey::idx_FinishHandlerLambda>;
        Functions["CombineCore"] = &TCallableConstraintTransformer::FromFinalLambda<TCoCombineCore::idx_FinishHandler>;
        Functions["PartitionByKey"] = &TCallableConstraintTransformer::PartitionByKeyWrap;
        Functions["PartitionsByKeys"] = &TCallableConstraintTransformer::PartitionByKeyWrap;
        Functions["GroupByKey"] = &TCallableConstraintTransformer::GroupByKeyWrap;
        Functions["Switch"] = &TCallableConstraintTransformer::SwitchWrap;
        Functions["Visit"] = &TCallableConstraintTransformer::VisitWrap;
        Functions["VariantItem"] = &TCallableConstraintTransformer::VariantItemWrap;
        Functions["Variant"] = &TCallableConstraintTransformer::VariantWrap;
        Functions["Guess"] = &TCallableConstraintTransformer::GuessWrap;
        Functions["Mux"] = &TCallableConstraintTransformer::MuxWrap;
        Functions["Nth"] = &TCallableConstraintTransformer::NthWrap;
        Functions["EquiJoin"] = &TCallableConstraintTransformer::EquiJoinWrap;
        Functions["MapJoinCore"] = &TCallableConstraintTransformer::MapJoinCoreWrap;
        Functions["CommonJoinCore"] = &TCallableConstraintTransformer::FromFirst<TEmptyConstraintNode>;
        Functions["ToDict"] = &TCallableConstraintTransformer::InheriteEmptyFromInput;
        Functions["FoldMap"] = &TCallableConstraintTransformer::InheriteEmptyFromInput; // TODO: passthrough
        Functions["Fold1Map"] = &TCallableConstraintTransformer::InheriteEmptyFromInput; // TODO: passthrough
        Functions["Chain1Map"] = &TCallableConstraintTransformer::InheriteEmptyFromInput; // TODO: passthrough, sorted, unique
        Functions["WideChain1Map"] = &TCallableConstraintTransformer::InheriteEmptyFromInput; // TODO: passthrough, sorted, unique
        Functions["IsKeySwitch"] = &TCallableConstraintTransformer::IsKeySwitchWrap;
        Functions["Condense"] = &TCallableConstraintTransformer::CondenseWrap;
        Functions["Condense1"] = &TCallableConstraintTransformer::CondenseWrap;
        Functions["Squeeze"] = &TCallableConstraintTransformer::InheriteEmptyFromInput;
        Functions["Squeeze1"] = &TCallableConstraintTransformer::InheriteEmptyFromInput;
        Functions["GroupingCore"] = &TCallableConstraintTransformer::InheriteEmptyFromInput;
        Functions["Chopper"] = &TCallableConstraintTransformer::InheriteEmptyFromInput;
        Functions["WideChopper"] = &TCallableConstraintTransformer::InheriteEmptyFromInput;
        Functions["WideCombiner"] = &TCallableConstraintTransformer::InheriteEmptyFromInput;
        Functions["WideCondense1"] = &TCallableConstraintTransformer::WideCondense1Wrap;
        Functions["Aggregate"] = &TCallableConstraintTransformer::AggregateWrap;
        Functions["AggregateMergeState"] = &TCallableConstraintTransformer::AggregateWrap;
        Functions["AggregateMergeFinalize"] = &TCallableConstraintTransformer::AggregateWrap;
        Functions["AggregateMergeManyFinalize"] = &TCallableConstraintTransformer::AggregateWrap;
        Functions["AggregateFinalize"] = &TCallableConstraintTransformer::AggregateWrap;
        Functions["Fold"] = &TCallableConstraintTransformer::FoldWrap;
        Functions["Fold1"] = &TCallableConstraintTransformer::FoldWrap;
        Functions["WithContext"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
        Functions["WithWorld"] = &TCallableConstraintTransformer::CopyAllFrom<0>;
    }

    std::optional<IGraphTransformer::TStatus> ProcessCore(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) {
        if (const auto func = Functions.find(input->Content()); Functions.cend() != func) {
            return (this->*func->second)(input, output, ctx);
        }
        return std::nullopt;
    }

    std::optional<IGraphTransformer::TStatus> ProcessList(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) {
        if (!input->ChildrenSize() || ETypeAnnotationKind::Tuple != input->GetTypeAnn()->GetKind())
            return TStatus::Ok;
        return AsTupleWrap(input, output, ctx);
    }

    TStatus ProcessUnknown(const TExprNode::TPtr& input, TExprContext&) {
        return UpdateAllChildLambdasConstraints(*input);
    }

    TStatus ValidateProviderCommitResult(const TExprNode::TPtr&, TExprContext&) {
        return TStatus::Ok;
    }

    TStatus ValidateProviderReadResult(const TExprNode::TPtr&, TExprContext&) {
        return TStatus::Ok;
    }

    TStatus ValidateProviderWriteResult(const TExprNode::TPtr&, TExprContext&) {
        return TStatus::Ok;
    }

    TStatus ValidateProviderConfigureResult(const TExprNode::TPtr&, TExprContext&) {
        return TStatus::Ok;
    }

    IGraphTransformer& GetTransformer(IDataProvider& provider) const {
        return provider.GetConstraintTransformer(InstantOnly, SubGraph);
    }

private:
    template <size_t Ndx>
    TStatus CopyAllFrom(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        Y_UNUSED(output);
        Y_UNUSED(ctx);
        input->CopyConstraints(*input->Child(Ndx));
        return TStatus::Ok;
    }

    template <class... TConstraints>
    TStatus FromFirst(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        Y_UNUSED(output);
        Y_UNUSED(ctx);
        TApplyConstraintFromInput<0, TConstraints...>::Do(input);
        return TStatus::Ok;
    }

    template <class... TConstraints>
    TStatus FromSecond(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        Y_UNUSED(output);
        Y_UNUSED(ctx);
        TApplyConstraintFromInput<1, TConstraints...>::Do(input);
        return TStatus::Ok;
    }

    template <size_t StartFromChild, class... TConstraints>
    TStatus CommonFromChildren(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        Y_UNUSED(output);
        TVector<const TConstraintSet*> constraints;
        for (size_t i = StartFromChild; i < input->ChildrenSize(); ++i) {
            constraints.push_back(&input->Child(i)->GetConstraintSet());
        }
        TApplyCommonConstraint<TConstraints...>::Do(input, constraints, ctx);
        return TStatus::Ok;
    }

    template <size_t StartFromChild>
    TStatus AllCommonFromChildren(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        Y_UNUSED(output);
        TVector<const TConstraintSet*> constraints;
        for (size_t i = StartFromChild; i < input->ChildrenSize(); ++i) {
            constraints.push_back(&input->Child(i)->GetConstraintSet());
        }
        TApplyCommonConstraint<TSortedConstraintNode
            , TUniqueConstraintNode
            , TPartOfUniqueConstraintNode
            , TPassthroughConstraintNode
            , TEmptyConstraintNode
            , TVarIndexConstraintNode
            , TMultiConstraintNode
            >::Do(input, constraints, ctx);
        return TStatus::Ok;
    }

    TStatus FromEmpty(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        input->AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
        return TStatus::Ok;
    }

    template <size_t LambdaIdx>
    TStatus FromFinalLambda(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        TStatus status = UpdateAllChildLambdasConstraints(*input);
        if (status != TStatus::Ok) {
            return status;
        }

        TApplyConstraintFromInput<LambdaIdx, TMultiConstraintNode, TEmptyConstraintNode>::Do(input);
        return FromFirst<TEmptyConstraintNode>(input, output, ctx);
    }

    TStatus InheriteEmptyFromInput(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        auto status = UpdateAllChildLambdasConstraints(*input);
        if (status != TStatus::Ok) {
            return status;
        }
        return FromFirst<TEmptyConstraintNode>(input, output, ctx);
    }

    TStatus SortWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        auto status = UpdateLambdaConstraints(*input->Child(2));
        if (status != TStatus::Ok) {
            return status;
        }

        if (auto sorted = DeduceSortConstraint(*input->Child(0), *input->Child(1), *input->Child(2), ctx)) {
            input->AddConstraint(sorted);
        }

        return FromFirst<TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TVarIndexConstraintNode>(input, output, ctx);
    }

    TStatus AssumeSortedWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        auto status = UpdateLambdaConstraints(*input->Child(2));
        if (status != TStatus::Ok) {
            return status;
        }

        if (auto assumeConstr = DeduceSortConstraint(*input->Child(0), *input->Child(1), *input->Child(2), ctx)) {
            input->AddConstraint(assumeConstr);
        }
        return FromFirst<TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TVarIndexConstraintNode>(input, output, ctx);
    }

    TStatus AssumeUniqueWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        TUniqueConstraintNode::TFullSetType sets;
        for (auto i = 1U; i < input->ChildrenSize(); ++i) {
            TUniqueConstraintNode::TSetType columns;
            columns.reserve(input->Child(i)->ChildrenSize());
            for (const auto& column: input->Child(i)->Children())
                columns.insert_unique(TConstraintNode::TPathType(1U, column->Content()));
            sets.insert_unique(std::move(columns));
        }

        if (sets.empty())
            sets.insert_unique(TUniqueConstraintNode::TSetType{TConstraintNode::TPathType()});

        input->AddConstraint(ctx.MakeConstraint<TUniqueConstraintNode>(std::move(sets)));
        return FromFirst<TPassthroughConstraintNode, TSortedConstraintNode, TEmptyConstraintNode, TVarIndexConstraintNode>(input, output, ctx);
    }

    template <bool UseSort>
    TStatus TopWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        auto status = UpdateLambdaConstraints(*input->Child(3));
        if (status != TStatus::Ok) {
            return status;
        }

        if constexpr (UseSort) {
            if (auto sorted = DeduceSortConstraint(*input->Child(0), *input->Child(2), *input->Child(3), ctx)) {
                input->AddConstraint(sorted);
            }
        }

        return FromFirst<TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TVarIndexConstraintNode>(input, output, ctx);
    }

    template <bool CheckMembersType = false>
    TStatus SelectMembersWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        const TTypeAnnotationNode* outItemType = input->GetTypeAnn();
        while (outItemType->GetKind() == ETypeAnnotationKind::Optional) {
            outItemType = outItemType->Cast<TOptionalExprType>()->GetItemType();
        }
        if (outItemType->GetKind() == ETypeAnnotationKind::Variant) {
            if (outItemType->Cast<TVariantExprType>()->GetUnderlyingType()->GetKind() == ETypeAnnotationKind::Tuple) {
                const auto outSize = outItemType->Cast<TVariantExprType>()->GetUnderlyingType()->Cast<TTupleExprType>()->GetSize();

                auto multi = input->Head().GetConstraint<TMultiConstraintNode>();
                if (multi && multi->GetItems().back().first >= outSize) {
                    TMultiConstraintNode::TMapType filteredItems;
                    for (auto& item: multi->GetItems()) {
                        if (item.first < outSize) {
                            filteredItems.push_back(item);
                        }
                    }
                    multi = filteredItems.empty() ? nullptr : ctx.MakeConstraint<TMultiConstraintNode>(std::move(filteredItems));
                }
                if (multi) {
                    input->AddConstraint(multi);
                }

                auto varIndex = input->Head().GetConstraint<TVarIndexConstraintNode>();
                if (varIndex && varIndex->GetIndexMapping().back().first >= outSize) {
                    TVarIndexConstraintNode::TMapType filteredItems;
                    for (auto& item: varIndex->GetIndexMapping()) {
                        if (item.first < outSize) {
                            filteredItems.push_back(item);
                        }
                    }
                    varIndex = filteredItems.empty() ? nullptr : ctx.MakeConstraint<TVarIndexConstraintNode>(std::move(filteredItems));
                }
                if (varIndex) {
                    input->AddConstraint(varIndex);
                }
            }
        }
        else if (outItemType->GetKind() == ETypeAnnotationKind::Struct) {
            const auto outStructType = outItemType->Cast<TStructExprType>();
            if (const auto passthrough = input->Head().GetConstraint<TPassthroughConstraintNode>()) {
                TPassthroughConstraintNode::TMapType filteredMapping;
                if constexpr (CheckMembersType) {
                    auto inItemType = input->Head().GetTypeAnn();
                    while (inItemType->GetKind() == ETypeAnnotationKind::Optional) {
                        inItemType = inItemType->Cast<TOptionalExprType>()->GetItemType();
                    }
                    const auto inStructType = inItemType->Cast<TStructExprType>();
                    const auto& inItems = inStructType->GetItems();
                    const auto& outItems = outStructType->GetItems();
                    for (const auto& part : passthrough->GetColumnMapping()) {
                        TPassthroughConstraintNode::TPartType filtered;
                        filtered.reserve(part.second.size());
                        for (const auto& item : part.second) {
                            if (!item.first.empty()) {
                                const auto outItem = outStructType->FindItem(item.first.front());
                                const auto inItem = inStructType->FindItem(item.first.front());
                                if (outItem && inItem && IsSameAnnotation(*outItems[*outItem]->GetItemType(), *inItems[*inItem]->GetItemType())) {
                                    filtered.push_back(item);
                                }
                            }
                            if (!filtered.empty()) {
                                filteredMapping.emplace(part.first ? part.first : passthrough, std::move(filtered));
                            }
                        }
                    }
                } else {
                    for (const auto& part: passthrough->GetColumnMapping()) {
                        TPassthroughConstraintNode::TPartType filtered;
                        filtered.reserve(part.second.size());
                        for (const auto& item : part.second) {
                            if (!item.first.empty() && outStructType->FindItem(item.first.front())) {
                                filtered.push_back(item);
                            }
                        }
                        if (!filtered.empty()) {
                            filteredMapping.emplace(part.first ? part.first : passthrough, std::move(filtered));
                        }
                    }
                }
                if (!filteredMapping.empty()) {
                    input->AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(filteredMapping)));
                }
            }

            if (const auto part = input->Head().GetConstraint<TPartOfUniqueConstraintNode>()) {
                auto mapping = part->GetColumnMapping();
                if constexpr (CheckMembersType) {
                    if (!mapping.empty()) {
                        auto inItemType = input->Head().GetTypeAnn();
                        while (inItemType->GetKind() == ETypeAnnotationKind::Optional) {
                            inItemType = inItemType->Cast<TOptionalExprType>()->GetItemType();
                        }
                        const auto inStructType = inItemType->Cast<TStructExprType>();
                        const auto& inItems = inStructType->GetItems();
                        const auto& outItems = outStructType->GetItems();
                        TPartOfUniqueConstraintNode::FilterFields(mapping, [&](const std::string_view& field) {
                            const auto outItem = outStructType->FindItem(field);
                            const auto inItem = inStructType->FindItem(field);
                            return outItem && inItem && IsSameAnnotation(*outItems[*outItem]->GetItemType(), *inItems[*inItem]->GetItemType());
                        });
                    }
                } else
                    TPartOfUniqueConstraintNode::FilterFields(mapping, [outStructType](const std::string_view& field) { return bool(outStructType->FindItem(field)); });

                if (!mapping.empty())
                    input->AddConstraint(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(mapping)));
            }
        }

        return TStatus::Ok;
    }

    TStatus DivePrefixMembersWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        const auto prefixes = input->Child(1)->Children();
        if (const auto passthrough = input->Head().GetConstraint<TPassthroughConstraintNode>()) {
            TPassthroughConstraintNode::TMapType filteredMapping;
            for (const auto& part: passthrough->GetColumnMapping()) {
                TPassthroughConstraintNode::TPartType filtered;
                filtered.reserve(part.second.size());
                for (auto item: part.second) {
                    for (const auto& p : prefixes) {
                        if (const auto& prefix = p->Content(); !item.first.empty() && item.first.front().starts_with(prefix)) {
                            item.first.front() = item.first.front().substr(prefix.length());
                            filtered.insert_unique(std::move(item));
                            break;
                        }
                    }
                }
                if (!filtered.empty()) {
                    filteredMapping.emplace(part.first ? part.first : passthrough, std::move(filtered));
                }
            }
            if (!filteredMapping.empty()) {
                input->AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(filteredMapping)));
            }
        }

        if (const auto part = input->Head().GetConstraint<TPartOfUniqueConstraintNode>()) {
            auto mapping = part->GetColumnMapping();
            for (auto part = mapping.begin(); mapping.end() != part;) {
                for (auto it = part->second.begin(); part->second.end() != it;) {
                    bool pass = false;
                    for (const auto& p : prefixes) {
                        const auto& prefix = p->Content();
                        pass = !it->first.empty() && it->first.front().starts_with(prefix);
                        if (pass) {
                            it->first.front() = it->first.front().substr(prefix.length());
                            break;
                        }
                    }

                    if (pass)
                        ++it;
                    else
                        it = part->second.erase(it);
                }

                if (part->second.empty())
                    part = mapping.erase(part);
                else
                    ++part;
            }
            if (!mapping.empty())
                input->AddConstraint(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(mapping)));
        }

        return FromFirst<TVarIndexConstraintNode>(input, output, ctx);
    }

    TStatus ExtractMembersWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        const auto outItemType = GetSeqItemType(*input->GetTypeAnn()).Cast<TStructExprType>();
        if (const auto passthrough = input->Head().GetConstraint<TPassthroughConstraintNode>()) {
            TPassthroughConstraintNode::TMapType filteredMapping;
            for (const auto& part: passthrough->GetColumnMapping()) {
                TPassthroughConstraintNode::TPartType filtered;
                filtered.reserve(part.second.size());
                for (const auto& item: part.second) {
                    if (!item.first.empty() && outItemType->FindItem(item.first.front())) {
                        filtered.push_back(item);
                    }
                }
                if (!filtered.empty()) {
                    filteredMapping.emplace(part.first ? part.first : passthrough, std::move(filtered));
                }
            }
            if (!filteredMapping.empty()) {
                input->AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(filteredMapping)));
            }
        }

        if (const auto sorted = TSortedConstraintNode::FilterByType(input->Head().GetConstraint<TSortedConstraintNode>(), outItemType, ctx))
            input->AddConstraint(sorted);

        if (const auto uniq = input->Head().GetConstraint<TUniqueConstraintNode>()) {
            if (const auto filtered = uniq->FilterFields(ctx, [outItemType](const TConstraintNode::TPathType& path) { return !path.empty() && outItemType->FindItem(path.front()); } )) {
                input->AddConstraint(filtered);
            }
        }

        if (const auto part = input->Head().GetConstraint<TPartOfUniqueConstraintNode>()) {
            if (const auto filtered = part->FilterFields(ctx, [outItemType](const TConstraintNode::TPathType& path) { return !path.empty() && outItemType->FindItem(path.front()); } )) {
                input->AddConstraint(filtered);
            }
        }

        return FromFirst<TEmptyConstraintNode, TVarIndexConstraintNode>(input, output, ctx);
    }

    TStatus RemovePrefixMembersWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        const TTypeAnnotationNode* outItemType = GetSeqItemType(input->GetTypeAnn());
        if (!outItemType) {
            outItemType = input->GetTypeAnn();
        }

        if (outItemType->GetKind() == ETypeAnnotationKind::Struct) {
            const auto outStructType = outItemType->Cast<TStructExprType>();
            if (const auto passthrough = input->Head().GetConstraint<TPassthroughConstraintNode>()) {
                TPassthroughConstraintNode::TMapType filteredMapping;
                for (const auto& part: passthrough->GetColumnMapping()) {
                    TPassthroughConstraintNode::TPartType filtered;
                    filtered.reserve(part.second.size());
                    for (const auto& item: part.second) {
                        if (!item.first.empty() && outStructType->FindItem(item.first.front())) {
                            filtered.push_back(item);
                        }
                    }
                    if (!filtered.empty()) {
                        filteredMapping.emplace(part.first ? part.first : passthrough, std::move(filtered));
                    }
                }
                if (!filteredMapping.empty()) {
                    input->AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(filteredMapping)));
                }
            }

            if (const auto sorted = TSortedConstraintNode::FilterByType(input->Head().GetConstraint<TSortedConstraintNode>(), outStructType, ctx))
                input->AddConstraint(sorted);

            if (const auto part = input->Head().GetConstraint<TPartOfUniqueConstraintNode>()) {
                auto mapping = part->GetColumnMapping();
                TPartOfUniqueConstraintNode::FilterFields(mapping, [outStructType](const std::string_view& field) { return bool(outStructType->FindItem(field)); });
                if (!mapping.empty())
                    input->AddConstraint(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(mapping)));
            }
        }
        else if (outItemType->GetKind() == ETypeAnnotationKind::Variant) {
            if (auto multi = input->Head().GetConstraint<TMultiConstraintNode>()) {
                TMultiConstraintNode::TMapType multiItems;
                auto tupleUnderType = outItemType->Cast<TVariantExprType>()->GetUnderlyingType()->Cast<TTupleExprType>();
                for (auto& item: multi->GetItems()) {
                    YQL_ENSURE(item.first < tupleUnderType->GetSize());

                    auto& constr = multiItems[item.first];
                    const TStructExprType* outStructType = tupleUnderType->GetItems()[item.first]->Cast<TStructExprType>();

                    if (const auto passthrough = item.second.GetConstraint<TPassthroughConstraintNode>()) {
                        TPassthroughConstraintNode::TMapType filteredMapping;
                        for (const auto& part: passthrough->GetColumnMapping()) {
                            TPassthroughConstraintNode::TPartType filtered;
                            filtered.reserve(part.second.size());
                            for (const auto& item: part.second) {
                                if (!item.first.empty() && outStructType->FindItem(item.first.front())) {
                                    filtered.push_back(item);
                                }
                            }
                            if (!filtered.empty()) {
                                filteredMapping.emplace(part.first ? part.first : passthrough, std::move(filtered));
                            }
                        }
                        if (!filteredMapping.empty()) {
                            constr.AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(filteredMapping)));
                        }
                    }

                    if (const auto sorted = TSortedConstraintNode::FilterByType(item.second.GetConstraint<TSortedConstraintNode>(), outStructType, ctx))
                        constr.AddConstraint(sorted);

                    if (const auto part = input->Head().GetConstraint<TPartOfUniqueConstraintNode>()) {
                        auto mapping = part->GetColumnMapping();
                        TPartOfUniqueConstraintNode::FilterFields(mapping, [outStructType](const std::string_view& field) { return bool(outStructType->FindItem(field)); });
                        if (!mapping.empty())
                            constr.AddConstraint(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(mapping)));
                    }
                }
                input->AddConstraint(ctx.MakeConstraint<TMultiConstraintNode>(std::move(multiItems)));
            }
        }

        return FromFirst<TEmptyConstraintNode, TVarIndexConstraintNode>(input, output, ctx);
    }

    // TODO: Empty for false condition
    template <bool UseSorted>
    TStatus FilterWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        if (const auto status = UpdateLambdaConstraints(*input->Child(1)); status != TStatus::Ok) {
            return status;
        }

        if constexpr (UseSorted) {
            FromFirst<TSortedConstraintNode>(input, output, ctx);
        }

        return FromFirst<TPassthroughConstraintNode, TEmptyConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>(input, output, ctx);
    }

    template<class TConstraint>
    static const TConstraint* GetConstraintFromWideResultLambda(const TExprNode& lambda, TExprContext& ctx);

    template<class TConstraintType>
    static const TConstraintType* GetLambdaConstraint(const TExprNode& lambda, TExprContext& ctx) {
        if (2U == lambda.ChildrenSize())
            return lambda.GetConstraint<TConstraintType>();

        TVector<const TConstraintSet*> constraints;
        constraints.reserve(lambda.ChildrenSize() - 1U);
        for (size_t i = 1U; i < lambda.ChildrenSize(); ++i) {
            constraints.emplace_back(&lambda.Child(i)->GetConstraintSet());
        }
        return TConstraintType::MakeCommon(constraints, ctx);
    }

    template<class TConstraintType, bool WideLambda>
    static const TConstraintType* GetConstraintFromLambda(const TExprNode& lambda, TExprContext& ctx) {
        if constexpr (WideLambda)
            return GetConstraintFromWideResultLambda<TConstraintType>(lambda, ctx);
        else
            return GetLambdaConstraint<TConstraintType>(lambda, ctx);
    }

    static TConstraintNode::TListType GetConstraintsForInputArgument(const TExprNode& node, std::unordered_set<const TPassthroughConstraintNode*>& explicitPasstrought, TExprContext& ctx) {
        TConstraintNode::TListType constraints;
        if (const auto inItemType = GetSeqItemType(node.Head().GetTypeAnn())) {
            if (inItemType->GetKind() == ETypeAnnotationKind::Variant) {
                if (inItemType->Cast<TVariantExprType>()->GetUnderlyingType()->GetKind() == ETypeAnnotationKind::Tuple) {
                    const auto tupleType = inItemType->Cast<TVariantExprType>()->GetUnderlyingType()->Cast<TTupleExprType>();
                    constraints.push_back(ctx.MakeConstraint<TVarIndexConstraintNode>(*inItemType->Cast<TVariantExprType>()));
                    TMultiConstraintNode::TMapType multiItems;
                    multiItems.reserve(tupleType->GetSize());
                    for (size_t i = 0; i < tupleType->GetSize(); ++i) {
                        multiItems.emplace_back(i, TConstraintSet{});
                        const auto inputMulti = node.Head().GetConstraint<TMultiConstraintNode>();
                        if (const auto inputConstr = inputMulti ? inputMulti->GetItem(i) : nullptr) {
                            if (auto mapping = TPartOfUniqueConstraintNode::GetCommonMapping(inputConstr->GetConstraint<TUniqueConstraintNode>(), inputConstr->GetConstraint<TPartOfUniqueConstraintNode>()); !mapping.empty()) {
                                multiItems.back().second.AddConstraint(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(mapping)));
                            }
                            if (const auto pass = inputConstr->GetConstraint<TPassthroughConstraintNode>()) {
                                multiItems.back().second.AddConstraint(pass);
                                continue;
                            }
                        }
                        switch (const auto variantItemType = tupleType->GetItems()[i]; variantItemType->GetKind()) {
                            case ETypeAnnotationKind::Tuple:
                                if (const auto argType = variantItemType->Cast<TTupleExprType>(); argType->GetSize() > 0U) {
                                    multiItems.back().second.AddConstraint(*explicitPasstrought.emplace(ctx.MakeConstraint<TPassthroughConstraintNode>(*argType)).first);
                                }
                                break;
                            case ETypeAnnotationKind::Struct:
                                if (const auto argType = variantItemType->Cast<TStructExprType>(); argType->GetSize() > 0U) {
                                    multiItems.back().second.AddConstraint(*explicitPasstrought.emplace(ctx.MakeConstraint<TPassthroughConstraintNode>(*argType)).first);
                                }
                                break;
                            default:
                                break;
                        }
                    }
                    if (!multiItems.empty()) {
                        constraints.emplace_back(ctx.MakeConstraint<TMultiConstraintNode>(std::move(multiItems)));
                    }
                }
            } else {
                if (const auto inputPassthrough = node.Head().GetConstraint<TPassthroughConstraintNode>()) {
                    constraints.emplace_back(inputPassthrough);
                } else switch (inItemType->GetKind()) {
                    case ETypeAnnotationKind::Tuple:
                        if (const auto argType = inItemType->Cast<TTupleExprType>(); argType->GetSize() > 0U) {
                            constraints.emplace_back(*explicitPasstrought.emplace(ctx.MakeConstraint<TPassthroughConstraintNode>(*argType)).first);
                        }
                        break;
                    case ETypeAnnotationKind::Struct:
                        if (const auto argType = inItemType->Cast<TStructExprType>(); argType->GetSize() > 0U) {
                            constraints.emplace_back(*explicitPasstrought.emplace(ctx.MakeConstraint<TPassthroughConstraintNode>(*argType)).first);
                        }
                        break;
                    default:
                        break;
                }

                if (auto mapping = TPartOfUniqueConstraintNode::GetCommonMapping(GetDetailedUnique(node.Head().GetConstraint<TUniqueConstraintNode>(), *node.Head().GetTypeAnn(), ctx), node.Head().GetConstraint<TPartOfUniqueConstraintNode>()); !mapping.empty()) {
                    constraints.emplace_back(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(mapping)));
                }
                if (const auto groupBy = node.Head().GetConstraint<TGroupByConstraintNode>()) {
                    constraints.emplace_back(groupBy);
                }
            }
        }

        return constraints;
    }

    template <bool UseSorted, bool Flat, bool WideInput = false, bool WideOutput = false>
    TStatus MapWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        const auto inItemType = GetSeqItemType(input->Head().GetTypeAnn());
        TSmallVec<TConstraintNode::TListType> argConstraints(input->Tail().Head().ChildrenSize());
        std::unordered_set<const TPassthroughConstraintNode*> explicitPasstrought;
        if constexpr (WideInput) {
            if (const auto& mapping = TPartOfUniqueConstraintNode::GetCommonMapping(input->Head().GetConstraint<TUniqueConstraintNode>(), input->Head().GetConstraint<TPartOfUniqueConstraintNode>()); !mapping.empty()) {
                for (ui32 i = 0U; i < argConstraints.size(); ++i) {
                    if (auto extracted = TPartOfUniqueConstraintNode::ExtractField(mapping, ctx.GetIndexAsString(i)); !extracted.empty()) {
                        argConstraints[i].emplace_back(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(extracted)));
                    }
                }
            }

            if (inItemType) {
                const auto multiType = inItemType->Cast<TMultiExprType>();
                const auto inputPassthrough = input->Head().GetConstraint<TPassthroughConstraintNode>();
                if (const auto passtrought = inputPassthrough ? inputPassthrough : multiType ->GetSize() > 0U ? *explicitPasstrought.emplace(ctx.MakeConstraint<TPassthroughConstraintNode>(*multiType)).first : nullptr) {
                    for (ui32 i = 0U; i < argConstraints.size(); ++i) {
                        if (const auto fieldPasstrought = passtrought->ExtractField(ctx, ctx.GetIndexAsString(i))) {
                            argConstraints[i].emplace_back(fieldPasstrought);
                        }
                    }
                }
            }
        } else {
            argConstraints.front() = GetConstraintsForInputArgument(*input, explicitPasstrought, ctx);
        }

        if (const auto status = UpdateLambdaConstraints(input->TailRef(), ctx, argConstraints); status != TStatus::Ok) {
            return status;
        }

        bool hasOutSorted = false;
        const auto lambdaPassthrough = GetConstraintFromLambda<TPassthroughConstraintNode, WideOutput>(input->Tail(), ctx);
        if (lambdaPassthrough) {
            if (!explicitPasstrought.contains(lambdaPassthrough)) {
                auto mapping = lambdaPassthrough->GetColumnMapping();
                for (const auto myPasstrought : explicitPasstrought) {
                    mapping.erase(myPasstrought);
                }
                if (!mapping.empty()) {
                    input->AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(mapping)));
                }
            }

            if constexpr (UseSorted) {
                if (const auto sorted = input->Head().GetConstraint<TSortedConstraintNode>()) {
                    if (const auto outSorted = GetPassthroughSortedConstraint(*sorted, *lambdaPassthrough, ctx)) {
                        input->AddConstraint(outSorted);
                        hasOutSorted = true;
                    }
                }
            }
        }

        if (const auto lambdaUnique = GetConstraintFromLambda<TPartOfUniqueConstraintNode, WideOutput>(input->Tail(), ctx)) {
            if (const auto unique = GetDetailedUnique(input->Head().GetConstraint<TUniqueConstraintNode>(), *input->Head().GetTypeAnn(), ctx)) {
                if (const auto complete = TPartOfUniqueConstraintNode::MakeComplete(ctx, lambdaUnique->GetColumnMapping(), unique)) {
                    input->AddConstraint(complete);
                }
            }
            if (const auto part = input->Head().GetConstraint<TPartOfUniqueConstraintNode>()) {
                auto mapping = lambdaUnique->GetColumnMapping();
                for (auto it = mapping.cbegin(); mapping.cend() != it;) {
                    if (part->GetColumnMapping().contains(it->first))
                        ++it;
                    else
                        it = mapping.erase(it);
                }
                if (!mapping.empty()) {
                    input->AddConstraint(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(mapping)));
                }
            }
        }
        const auto lambdaVarIndex = GetConstraintFromLambda<TVarIndexConstraintNode, WideOutput>(input->Tail(), ctx);
        const auto lambdaMulti = GetConstraintFromLambda<TMultiConstraintNode, WideOutput>(input->Tail(), ctx);
        const bool multiInput = ETypeAnnotationKind::Variant == inItemType->GetKind();
        if (const auto varIndex = input->Head().GetConstraint<TVarIndexConstraintNode>()) {
            if (multiInput) {
                if (lambdaVarIndex) {
                    if (const auto outVarIndex = GetVarIndexOverVarIndexConstraint(*varIndex, *lambdaVarIndex, ctx)) {
                        input->AddConstraint(outVarIndex);
                    }
                }
            } else {
                if (lambdaMulti) {
                    TVarIndexConstraintNode::TMapType remapItems;
                    for (auto& multiItem: lambdaMulti->GetItems()) {
                        for (auto& varItem: varIndex->GetIndexMapping()) {
                            remapItems.push_back(std::make_pair(multiItem.first, varItem.second));
                        }
                    }
                    if (!remapItems.empty()) {
                        ::SortUnique(remapItems);
                        input->AddConstraint(ctx.MakeConstraint<TVarIndexConstraintNode>(std::move(remapItems)));
                    }
                } else {
                    input->AddConstraint(varIndex);
                }
            }
        }

        const auto inputMulti = input->Head().GetConstraint<TMultiConstraintNode>();
        if (lambdaMulti && !input->Head().GetConstraint<TEmptyConstraintNode>()) {
            TMultiConstraintNode::TMapType remappedItems;
            for (auto& item: lambdaMulti->GetItems()) {
                remappedItems.push_back(std::make_pair(item.first, TConstraintSet{}));
                if (!multiInput) { // remapping one to many
                    if (const auto lambdaPassthrough = item.second.template GetConstraint<TPassthroughConstraintNode>()) {
                        if (!explicitPasstrought.contains(lambdaPassthrough)) {
                            auto mapping = lambdaPassthrough->GetColumnMapping();
                            for (const auto myPasstrought : explicitPasstrought)
                                mapping.erase(myPasstrought);
                            if (!mapping.empty()) {
                                remappedItems.back().second.AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(mapping)));
                            }
                        }

                        if constexpr (UseSorted) {
                            if (const auto sorted = input->Head().GetConstraint<TSortedConstraintNode>()) {
                                if (const auto outSorted = GetPassthroughSortedConstraint(*sorted, *lambdaPassthrough, ctx)) {
                                    remappedItems.back().second.AddConstraint(outSorted);
                                }
                            }
                        }
                    }
                    if (input->Head().GetConstraint<TUniqueConstraintNode>()) {
                        if (const auto lambdaUnique = item.second.template GetConstraint<TUniqueConstraintNode>()) {
                            remappedItems.back().second.AddConstraint(lambdaUnique);
                        }
                    }

                    if (const auto empty = item.second.template GetConstraint<TEmptyConstraintNode>()) {
                        remappedItems.pop_back();
                    }
                }
                else if (lambdaVarIndex && inputMulti) {
                    const auto range = lambdaVarIndex->GetIndexMapping().equal_range(item.first);
                    switch (std::distance(range.first, range.second)) {
                    case 0: // new index
                        break;
                    case 1: // remapping 1 to 1
                        if (const auto origConstr = inputMulti->GetItem(range.first->second)) {
                            if (const auto lambdaPassthrough = item.second.template GetConstraint<TPassthroughConstraintNode>()) {
                                if (!explicitPasstrought.contains(lambdaPassthrough)) {
                                    auto mapping = lambdaPassthrough->GetColumnMapping();
                                    for (const auto myPasstrought : explicitPasstrought)
                                        mapping.erase(myPasstrought);
                                    if (!mapping.empty()) {
                                        remappedItems.back().second.AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(mapping)));
                                    }
                                }

                                if constexpr (UseSorted) {
                                    if (const auto sorted = origConstr->template GetConstraint<TSortedConstraintNode>()) {
                                        if (auto outSorted = GetPassthroughSortedConstraint(*sorted, *lambdaPassthrough, ctx)) {
                                            remappedItems.back().second.AddConstraint(outSorted);
                                        }
                                    }
                                }
                            }
                            if (origConstr->template GetConstraint<TUniqueConstraintNode>()) {
                                if (const auto lambdaUnique = item.second.template GetConstraint<TUniqueConstraintNode>()) {
                                    remappedItems.back().second.AddConstraint(lambdaUnique);
                                }
                            }
                            if (const auto empty = item.second.template GetConstraint<TEmptyConstraintNode>()) {
                                remappedItems.pop_back();
                            }
                        } else {
                            remappedItems.pop_back();
                        }
                        break;
                    default: // remapping many to one
                        {
                            std::vector<const TConstraintSet*> nonEmpty;
                            for (auto i = range.first; i != range.second; ++i) {
                                if (auto origConstr = inputMulti->GetItem(i->second)) {
                                    nonEmpty.push_back(origConstr);
                                }
                            }
                            EraseIf(nonEmpty, [] (const TConstraintSet* c) { return !!c->GetConstraint<TEmptyConstraintNode>(); });

                            if (nonEmpty.empty()) {
                                remappedItems.back().second.AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
                            } else if (nonEmpty.size() == 1) {
                                remappedItems.back().second = std::move(*nonEmpty.front());
                            }
                        }
                    }
                } else {
                    remappedItems.back().second = item.second;
                }
            }
            if (remappedItems) {
                input->AddConstraint(ctx.MakeConstraint<TMultiConstraintNode>(std::move(remappedItems)));
            }
        }
        else if (inputMulti && lambdaVarIndex) { // Many to one
            const auto range = lambdaVarIndex->GetIndexMapping().equal_range(0);
            std::vector<const TConstraintSet*> nonEmpty;
            for (auto i = range.first; i != range.second; ++i) {
                if (auto origConstr = inputMulti->GetItem(i->second)) {
                    nonEmpty.push_back(origConstr);
                }
            }
            EraseIf(nonEmpty, [] (const TConstraintSet* c) { return !!c->GetConstraint<TEmptyConstraintNode>(); });

            if (nonEmpty.empty()) {
                input->AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
            } else if (nonEmpty.size() == 1) {
                input->SetConstraints(*nonEmpty.front());
            }
        }

        if (const auto lambdaEmpty = GetConstraintFromLambda<TEmptyConstraintNode, WideOutput>(input->Tail(), ctx)) {
            if (TCoFlatMapBase::Match(input.Get())) {
                input->AddConstraint(lambdaEmpty);
            }
            if (UseSorted && !hasOutSorted && !lambdaPassthrough) {
                if (const auto sorted = input->Head().GetConstraint<TSortedConstraintNode>()) {
                    if (const auto outItemType = GetSeqItemType(input->GetTypeAnn())) {
                        if (outItemType->GetKind() == ETypeAnnotationKind::Struct) {
                            if (const auto outSorted = TSortedConstraintNode::FilterByType(sorted, outItemType->Cast<TStructExprType>(), ctx)) {
                                input->AddConstraint(outSorted);
                            }
                        }
                    }
                }
            }
        }

        return FromFirst<TEmptyConstraintNode>(input, output, ctx);
    }

    template <bool UseSorted>
    TStatus LMapWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        TConstraintNode::TListType argConstraints;
        for (const auto c: input->Head().GetAllConstraints()) {
            if (UseSorted || c->GetName() != TSortedConstraintNode::Name()) {
                argConstraints.push_back(c);
            }
        }

        if (const auto status = UpdateLambdaConstraints(input->TailRef(), ctx, {argConstraints}); status != TStatus::Ok) {
            return status;
        }

        TSet<TStringBuf> except;
        if (!UseSorted) {
            except.insert(TSortedConstraintNode::Name());
        }
        if (input->Tail().GetTypeAnn()->GetKind() == ETypeAnnotationKind::Optional) {
            except.insert(TEmptyConstraintNode::Name());
        }
        CopyExcept(*input, input->Tail(), except);

        return FromFirst<TEmptyConstraintNode>(input, output, ctx);
    }

    template <bool AsList>
    TStatus ExtendWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        if (input->ChildrenSize() == 1) {
            if (const auto unique = input->Head().GetConstraint<TUniqueConstraintNode>()) {
                input->AddConstraint(unique);
            }
            if (const auto part = input->Head().GetConstraint<TPartOfUniqueConstraintNode>()) {
                input->AddConstraint(part);
            }
        }

        if (AsList) {
            return CommonFromChildren<0, TPassthroughConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>(input, output, ctx);
        } else {
            return CommonFromChildren<0, TPassthroughConstraintNode, TEmptyConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>(input, output, ctx);
        }
    }

    template <bool Union>
    TStatus MergeWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        if (auto sort = MakeCommonConstraint<TSortedConstraintNode>(input, 0, ctx)) {
            if (Union && input->ChildrenSize() > 1) {
                // Check and exclude modified keys from final constraint
                auto resultStruct = input->GetTypeAnn()->Cast<TListExprType>()->GetItemType()->Cast<TStructExprType>();
                std::vector<const TStructExprType*> inputStructs;
                for (auto& child: input->Children()) {
                    inputStructs.push_back(child->GetTypeAnn()->Cast<TListExprType>()->GetItemType()->Cast<TStructExprType>());
                }
                auto commonPrefixLength = sort->GetContent().size();
                for (size_t i = 0; i < commonPrefixLength; ++i) {
                    auto column = sort->GetContent()[i].first.front();
                    auto pos = resultStruct->FindItem(column);
                    YQL_ENSURE(pos, "Missing column " << TString{column}.Quote() << " in result type");
                    auto resultItemType = resultStruct->GetItems()[*pos];
                    for (size_t childNdx = 0; childNdx < input->ChildrenSize(); ++childNdx) {
                        const auto inputStruct = inputStructs[childNdx];
                        if (auto pos = inputStruct->FindItem(column)) {
                            if (resultItemType != inputStruct->GetItems()[*pos]) {
                                commonPrefixLength = i;
                                break;
                            }
                        } else {
                            YQL_ENSURE(input->Child(childNdx)->GetConstraint<TEmptyConstraintNode>(), "Missing column " << TString{column}.Quote() << " in non empty input type");
                        }
                    }
                }
                sort = sort->CutPrefix(commonPrefixLength, ctx);
            }
            if (sort) {
                input->AddConstraint(sort);
            }
        }
        return ExtendWrap<false>(input, output, ctx);
    }

    TStatus OrderedExtendWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        if (input->ChildrenSize() == 1) {
            if (auto sorted = input->Head().GetConstraint<TSortedConstraintNode>()) {
                input->AddConstraint(sorted);
            }
        }

        return ExtendWrap<false>(input, output, ctx);
    }

    TStatus TakeWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        if (input->Tail().IsCallable("Uint64") && !FromString<ui64>(input->Tail().Head().Content())) {
            input->AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
        }
        return CopyAllFrom<0>(input, output, ctx);
    }

    TStatus MemberWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        const auto& memberName = input->Tail().Content();
        const auto& structNode = input->Head();
        if (const auto structPassthrough = structNode.GetConstraint<TPassthroughConstraintNode>()) {
            if (const auto p = structPassthrough->ExtractField(ctx, memberName)) {
                input->AddConstraint(p);
            }
        }
        if (const auto emptyConstraint = structNode.GetConstraint<TEmptyConstraintNode>()) {
            input->AddConstraint(emptyConstraint);
        } else if (const auto part = structNode.GetConstraint<TPartOfUniqueConstraintNode>()) {
            if (const auto extracted = part->ExtractField(ctx, memberName)) {
                input->AddConstraint(extracted);
            }
        }

        if (structNode.IsCallable("AsStruct")) {
            for (const auto& child: structNode.Children()) {
                if (child->Head().IsAtom(memberName)) {
                    TApplyConstraintFromInput<1, TVarIndexConstraintNode>::Do(child);
                    break;
                }
            }
        } else {
            TApplyConstraintFromInput<0, TVarIndexConstraintNode>::Do(input);
        }
        return TStatus::Ok;
    }

    TStatus AsTupleWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        TPassthroughConstraintNode::TMapType passthrough;
        TPartOfUniqueConstraintNode::TMapType uniques;

        std::vector<const TConstraintSet*> structConstraints;
        for (auto i = 0U; i < input->ChildrenSize(); ++i) {
            const auto child = input->Child(i);
            const auto& name = ctx.GetIndexAsString(i);
            if (const auto pass = child->GetConstraint<TPassthroughConstraintNode>()) {
                for (auto part : pass->GetColumnMapping()) {
                    std::transform(part.second.cbegin(), part.second.cend(), std::back_inserter(passthrough[part.first ? part.first : pass]), [&name](TPassthroughConstraintNode::TPartType::value_type item) {
                        item.first.emplace_front(name);
                        return item;
                    });
                }
            }

            if (const auto part = child->GetConstraint<TPartOfUniqueConstraintNode>()) {
                TPartOfUniqueConstraintNode::UniqueMerge(uniques, part->GetColumnMapping(name));
            }

            if (const auto& valueNode = SkipModifiers(child); TCoMember::Match(valueNode) || TCoNth::Match(valueNode)) {
                structConstraints.push_back(&valueNode->Head().GetConstraintSet());
            } else if (valueNode->IsArgument() && ETypeAnnotationKind::Struct != valueNode->GetTypeAnn()->GetKind()) {
                structConstraints.push_back(&valueNode->GetConstraintSet());
            }
        }
        if (!passthrough.empty()) {
            input->AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(passthrough)));
        }
        if (!uniques.empty()) {
            input->AddConstraint(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(uniques)));
        }
        if (const auto varIndex = TVarIndexConstraintNode::MakeCommon(structConstraints, ctx)) {
            input->AddConstraint(varIndex);
        }

        return TStatus::Ok;
    }

    TStatus AsStructWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        TPassthroughConstraintNode::TMapType passthrough;
        TPartOfUniqueConstraintNode::TMapType uniques;

        std::vector<const TConstraintSet*> structConstraints;
        for (const auto& child : input->Children()) {
            const auto& name = child->Head().Content();
            if (const auto pass = child->Tail().GetConstraint<TPassthroughConstraintNode>()) {
                for (auto part : pass->GetColumnMapping()) {
                    std::transform(part.second.cbegin(), part.second.cend(), std::back_inserter(passthrough[part.first ? part.first : pass]), [&name](TPassthroughConstraintNode::TPartType::value_type item) {
                        item.first.emplace_front(name);
                        return item;
                    });
                }
            }

            if (const auto part = child->Tail().GetConstraint<TPartOfUniqueConstraintNode>()) {
                TPartOfUniqueConstraintNode::UniqueMerge(uniques, part->GetColumnMapping(name));
            }

            if (const auto valueNode = SkipModifiers(&child->Tail()); TCoMember::Match(valueNode) || TCoNth::Match(valueNode)) {
                structConstraints.push_back(&valueNode->Head().GetConstraintSet());
            } else if (valueNode->Type() == TExprNode::Argument) {
                structConstraints.push_back(&valueNode->GetConstraintSet());
            }
        }
        if (!passthrough.empty()) {
            input->AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(passthrough)));
        }
        if (!uniques.empty()) {
            input->AddConstraint(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(uniques)));
        }
        if (const auto varIndex = TVarIndexConstraintNode::MakeCommon(structConstraints, ctx)) {
            input->AddConstraint(varIndex);
        }

        return TStatus::Ok;
    }

    TStatus AddMemberWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        const auto& addStructNode = input->Head();
        const auto& extraFieldNode = input->Tail();
        const auto& name = input->Child(1)->Content();

        if (const auto structPassthrough = addStructNode.GetConstraint<TPassthroughConstraintNode>(), fieldPasstrought = extraFieldNode.GetConstraint<TPassthroughConstraintNode>(); fieldPasstrought) {
            auto mapping = structPassthrough ? structPassthrough->GetColumnMapping() : TPassthroughConstraintNode::TMapType();
            if (const auto self = mapping.find(nullptr); mapping.cend() != self)
                mapping.emplace(structPassthrough, std::move(mapping.extract(self).mapped()));
            for (const auto& part : fieldPasstrought->GetColumnMapping()) {
                for (auto item : part.second) {
                    item.first.emplace_front(name);
                    mapping[part.first ? part.first : fieldPasstrought].insert_unique(std::move(item));
                }
            }

            input->AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(mapping)));
        } else if (structPassthrough) {
            input->AddConstraint(structPassthrough);
        }

        if (const auto emptyConstraint = addStructNode.GetConstraint<TEmptyConstraintNode>()) {
            input->AddConstraint(emptyConstraint);
        }

        TPartOfUniqueConstraintNode::TMapType uniques;
        if (const auto part = input->Head().GetConstraint<TPartOfUniqueConstraintNode>()) {
            uniques = part->GetColumnMapping();
        }
        if (const auto part = input->Tail().GetConstraint<TPartOfUniqueConstraintNode>()) {
            TPartOfUniqueConstraintNode::UniqueMerge(uniques, part->GetColumnMapping(name));
        }
        if (!uniques.empty()) {
            input->AddConstraint(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(uniques)));
        }

        TVector<const TConstraintSet*> structConstraints;
        structConstraints.push_back(&addStructNode.GetConstraintSet());
        if (const auto& valueNode = SkipModifiers(&extraFieldNode); TCoMember::Match(valueNode) || TCoNth::Match(valueNode)) {
            structConstraints.push_back(&valueNode->Head().GetConstraintSet());
        } else if (valueNode->Type() == TExprNode::Argument) {
            structConstraints.push_back(&valueNode->GetConstraintSet());
        }

        if (const auto varIndex = TVarIndexConstraintNode::MakeCommon(structConstraints, ctx)) {
            input->AddConstraint(varIndex);
        }
        return TStatus::Ok;
    }

    TStatus RemoveMemberWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        const auto& name = input->Tail().Content();
        if (const auto structPassthrough = input->Head().GetConstraint<TPassthroughConstraintNode>()) {
            const TConstraintNode::TPathType key(1U, name);
            auto mapping = structPassthrough->GetColumnMapping();
            if (const auto self = mapping.find(nullptr); mapping.cend() != self)
                mapping.emplace(structPassthrough, std::move(mapping.extract(self).mapped()));
            for (auto p = mapping.begin(); mapping.end() != p;) {
                if (auto it = p->second.lower_bound(key); p->second.cend() > it && it->first.front() == key.front()) {
                    do p->second.erase(it++);
                    while (p->second.end() > it && it->first.front() == key.front());
                    if (p->second.empty()) {
                        mapping.erase(p++);
                        continue;
                    }
                }
                ++p;
            }
            if (!mapping.empty()) {
                input->AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(mapping)));
            }
        }

        if (const auto part = input->Head().GetConstraint<TPartOfUniqueConstraintNode>()) {
            auto mapping = part->GetColumnMapping();
            TPartOfUniqueConstraintNode::FilterFields(mapping, std::bind(std::not_equal_to<std::string_view>(), std::cref(name), std::placeholders::_1));
            if (!mapping.empty())
                input->AddConstraint(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(mapping)));
        }

        return FromFirst<TVarIndexConstraintNode>(input, output, ctx);
    }

    TStatus ReplaceMemberWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        const auto& name = input->Child(1)->Content();
        TVector<const TConstraintSet*> structConstraints;
        structConstraints.push_back(&input->Head().GetConstraintSet());

        if (const auto structPassthrough = input->Head().GetConstraint<TPassthroughConstraintNode>(), fieldPasstrought = input->Tail().GetConstraint<TPassthroughConstraintNode>();
            structPassthrough || fieldPasstrought) {
            auto mapping = structPassthrough ? structPassthrough->GetColumnMapping() : TPassthroughConstraintNode::TMapType();
            if (const auto self = mapping.find(nullptr); mapping.cend() != self)
                mapping.emplace(structPassthrough, std::move(mapping.extract(self).mapped()));
            const TConstraintNode::TPathType key(1U, name);
            for (auto p = mapping.begin(); mapping.end() != p;) {
                if (auto it = p->second.lower_bound(key); p->second.cend() > it && it->first.front() == key.front()) {
                    do p->second.erase(it++);
                    while (p->second.end() > it && it->first.front() == key.front());
                    if (p->second.empty()) {
                        mapping.erase(p++);
                        continue;
                    }
                }
                ++p;
            }
            if (fieldPasstrought) {
                for (const auto& part : fieldPasstrought->GetColumnMapping()) {
                    for (auto item : part.second) {
                        item.first.emplace_front(name);
                        mapping[part.first ? part.first : fieldPasstrought].insert_unique(std::move(item));
                    }
                }
            }

            if (!mapping.empty()) {
                input->AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(mapping)));
            }

            if (structPassthrough && fieldPasstrought) {
                if (const auto valueNode = SkipModifiers(&input->Tail()); TCoMember::Match(valueNode) || TCoNth::Match(valueNode)) {
                    structConstraints.push_back(&valueNode->Head().GetConstraintSet());
                }
            }
        }

        TPartOfUniqueConstraintNode::TMapType uniques;
        if (const auto part = input->Head().GetConstraint<TPartOfUniqueConstraintNode>()) {
            uniques = part->GetColumnMapping();
            TPartOfUniqueConstraintNode::FilterFields(uniques, std::bind(std::not_equal_to<std::string_view>(), std::cref(name), std::placeholders::_1));
        }
        if (const auto part = input->Tail().GetConstraint<TPartOfUniqueConstraintNode>()) {
            TPartOfUniqueConstraintNode::UniqueMerge(uniques, part->GetColumnMapping(name));
        }
        if (!uniques.empty()) {
            input->AddConstraint(ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(uniques)));
        }

        if (const auto varIndex = TVarIndexConstraintNode::MakeCommon(structConstraints, ctx)) {
            input->AddConstraint(varIndex);
        }

        return TStatus::Ok;
    }

    TStatus ListWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        if (input->ChildrenSize() == 1) {
            return FromEmpty(input, output, ctx);
        } else if (input->ChildrenSize() == 2) {
            return FromSecond<TPassthroughConstraintNode, TUniqueConstraintNode, TPartOfUniqueConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>(input, output, ctx);
        }
        else if (input->ChildrenSize() > 2) {
            return CommonFromChildren<1, TPassthroughConstraintNode, TVarIndexConstraintNode, TMultiConstraintNode>(input, output, ctx);
        }
        return TStatus::Ok;
    }

    TStatus DictWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        if (input->ChildrenSize() == 1) {
            return FromEmpty(input, output, ctx);
        }
        return TStatus::Ok;
    }

    TStatus DictFromKeysWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        if (input->Child(1)->ChildrenSize() == 0) {
            input->AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
        }
        return TStatus::Ok;
    }

    TStatus IfWrap(const TExprNode::TPtr& input, TExprNode::TPtr&, TExprContext& ctx) const {
        std::vector<const TConstraintSet*> constraints;
        constraints.reserve((input->ChildrenSize() << 1U) + 1U);
        if (1U != input->Tail().ChildrenSize() || !input->Tail().IsCallable({"List", "Nothing"}))
            constraints.emplace_back(&input->Tail().GetConstraintSet());

        for (auto i = 0U; i < input->ChildrenSize() - 1U; ++i) {
            if (const auto child = input->Child(++i); 1U != child->ChildrenSize() || !child->IsCallable({"List", "Nothing"})) {// TODO: Use empty constraint.
                constraints.emplace_back(&child->GetConstraintSet());
            }
        }

        if (constraints.empty())
            input->AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
        else if (1U == constraints.size())
            input->SetConstraints(**constraints.cbegin());
        else
            TApplyCommonConstraint<TSortedConstraintNode
                , TUniqueConstraintNode
                , TPartOfUniqueConstraintNode
                , TPassthroughConstraintNode
                , TEmptyConstraintNode
                , TVarIndexConstraintNode
                , TMultiConstraintNode
                >::Do(input, constraints, ctx);
        return TStatus::Ok;
    }

    TStatus IfPresentWrap(const TExprNode::TPtr& input, TExprNode::TPtr&, TExprContext& ctx) const {
        auto optionals = input->ChildrenList();
        const auto lambdaIndex = optionals.size() - 2U;
        auto lambda = std::move(optionals[lambdaIndex]);
        optionals.resize(lambdaIndex);

        std::vector<const TConstraintNode::TListType> constraints;
        constraints.reserve(optionals.size());
        std::transform(optionals.cbegin(), optionals.cend(), std::back_inserter(constraints), [](const TExprNode::TPtr& node){ return node->GetAllConstraints(); });

        if (const auto status = UpdateLambdaConstraints(input->ChildRef(lambdaIndex), ctx, constraints); status != TStatus::Ok) {
            return status;
        }

        if (std::any_of(optionals.cbegin(), optionals.cend(), [] (const TExprNode::TPtr& node) { return bool(node->GetConstraint<TEmptyConstraintNode>()); })) {
            input->CopyConstraints(input->Tail());
            return TStatus::Ok;
        }

        const std::vector<const TConstraintSet*> both = { &lambda->GetConstraintSet(), &input->Tail().GetConstraintSet() };
        TApplyCommonConstraint<TSortedConstraintNode
            , TUniqueConstraintNode
            , TPartOfUniqueConstraintNode
            , TPassthroughConstraintNode
            , TEmptyConstraintNode
            , TVarIndexConstraintNode
            , TMultiConstraintNode
            >::Do(input, both, ctx);
        return TStatus::Ok;
    }

    TStatus SwitchWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        TStatus status = TStatus::Ok;
        TDynBitMap outFromChildren; // children, from which take a multi constraint for output
        if (const auto multi = input->Head().GetConstraint<TMultiConstraintNode>()) {
            for (size_t i = 2; i < input->ChildrenSize(); ++i) {
                TMultiConstraintNode::TMapType items;
                ui32 lambdaInputIndex = 0;
                for (auto& child : input->Child(i)->Children()) {
                    const ui32 index = FromString<ui32>(child->Content());
                    if (auto c = multi->GetItem(index)) {
                        items[lambdaInputIndex] = *c;
                        outFromChildren.Set(i + 1);
                    }
                    ++lambdaInputIndex;
                }
                TConstraintNode::TListType argConstraints;
                if (!items.empty()) {
                    if (input->Child(i)->ChildrenSize() > 1) {
                        argConstraints.push_back(ctx.MakeConstraint<TMultiConstraintNode>(std::move(items)));
                        argConstraints.push_back(ctx.MakeConstraint<TVarIndexConstraintNode>(input->Child(i)->ChildrenSize()));
                    } else {
                        argConstraints = items.front().second.GetAllConstraints();
                    }
                }

                status = status.Combine(UpdateLambdaConstraints(input->ChildRef(++i), ctx, {argConstraints}));
            }
        } else {
            const bool inVar = GetSeqItemType(*input->Head().GetTypeAnn()).GetKind() == ETypeAnnotationKind::Variant;
            const TSmallVec<TConstraintNode::TListType> argConstraints(1U, inVar ? TConstraintNode::TListType() : input->Head().GetAllConstraints());
            for (size_t i = 3; i < input->ChildrenSize(); i += 2) {
                status = status.Combine(UpdateLambdaConstraints(input->ChildRef(i), ctx, argConstraints));
            }
            outFromChildren.Set(0, input->ChildrenSize());
        }

        if (status != TStatus::Ok) {
            return status;
        }

        const auto inputVarIndex = input->Head().GetConstraint<TVarIndexConstraintNode>();
        const bool emptyInput = input->Head().GetConstraint<TEmptyConstraintNode>();
        if (GetSeqItemType(*input->GetTypeAnn()).GetKind() == ETypeAnnotationKind::Variant) {
            ui32 outIndexOffset = 0;
            TMultiConstraintNode::TMapType multiItems;
            TVarIndexConstraintNode::TMapType remapItems;
            bool emptyOut = true;
            for (size_t i = 2; i < input->ChildrenSize(); i += 2) {
                const auto lambda = input->Child(i + 1);
                const auto& lambdaItemType = GetSeqItemType(*lambda->GetTypeAnn());

                if (inputVarIndex) {
                    if (auto varIndex = lambda->GetConstraint<TVarIndexConstraintNode>()) {
                        for (auto& item: varIndex->GetIndexMapping()) {
                            YQL_ENSURE(item.second < input->Child(i)->ChildrenSize());
                            const auto srcIndex = FromString<size_t>(input->Child(i)->Child(item.second)->Content());
                            remapItems.push_back(std::make_pair(outIndexOffset + item.first, srcIndex));
                        }
                    } else if (lambdaItemType.GetKind() == ETypeAnnotationKind::Variant && input->Child(i)->ChildrenSize() == 1) {
                        const auto srcIndex = FromString<size_t>(input->Child(i)->Head().Content());
                        for (size_t j = 0; j < lambdaItemType.Cast<TVariantExprType>()->GetUnderlyingType()->Cast<TTupleExprType>()->GetSize(); ++j) {
                            remapItems.push_back(std::make_pair(outIndexOffset + j, srcIndex));
                        }
                    } else if (lambdaItemType.GetKind() != ETypeAnnotationKind::Variant && input->Child(i)->ChildrenSize() > 1) {
                        for (auto& child : input->Child(i)->Children()) {
                            const auto srcIndex = FromString<size_t>(child->Content());
                            remapItems.push_back(std::make_pair(outIndexOffset, srcIndex));
                        }
                    }
                }

                const bool lambdaEmpty = lambda->GetConstraint<TEmptyConstraintNode>();
                if (!lambdaEmpty) {
                    emptyOut = false;
                }
                if (lambdaItemType.GetKind() == ETypeAnnotationKind::Variant) {
                    if (!emptyInput && outFromChildren.Test(i + 1)) {
                        if (auto multi = lambda->GetConstraint<TMultiConstraintNode>()) {
                            for (auto& item: multi->GetItems()) {
                                multiItems.insert_unique(std::make_pair(outIndexOffset + item.first, item.second));
                            }
                        }
                    }
                    outIndexOffset += lambdaItemType.Cast<TVariantExprType>()->GetUnderlyingType()->Cast<TTupleExprType>()->GetSize();
                } else {
                    if (!emptyInput && outFromChildren.Test(i + 1) && !lambdaEmpty) {
                        multiItems[outIndexOffset] = lambda->GetConstraintSet();
                    }
                    ++outIndexOffset;
                }
            }

            if (inputVarIndex && !remapItems.empty()) {
                TVarIndexConstraintNode::TMapType result;
                for (auto& item: remapItems) {
                    auto range = inputVarIndex->GetIndexMapping().equal_range(item.second);
                    for (auto it = range.first; it != range.second; ++it) {
                        result.push_back(std::make_pair(item.first, it->second));
                    }
                }
                if (!result.empty()) {
                    ::Sort(result);
                    input->AddConstraint(ctx.MakeConstraint<TVarIndexConstraintNode>(std::move(result)));
                }
            }

            if (!multiItems.empty()) {
                input->AddConstraint(ctx.MakeConstraint<TMultiConstraintNode>(std::move(multiItems)));
            }
            if (emptyOut) {
                input->AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
            }
        } else {
            YQL_ENSURE(input->ChildrenSize() == 4);
            input->CopyConstraints(*input->Child(3));
        }
        return FromFirst<TEmptyConstraintNode>(input, output, ctx);
    }

    TStatus VisitWrap(const TExprNode::TPtr& input, TExprNode::TPtr&, TExprContext& ctx) const {
        TStatus status = TStatus::Ok;
        TDynBitMap outFromChildren; // children, from which take a multi constraint for output
        TDynBitMap usedAlts;
        const auto inMulti = input->Head().GetConstraint<TMultiConstraintNode>();
        for (ui32 i = 1; i < input->ChildrenSize(); ++i) {
            if (const auto child = input->Child(i); child->IsAtom()) {
                TSmallVec<TConstraintNode::TListType> argConstraints(1U);
                if (inMulti) {
                    const auto index = FromString<ui32>(child->Content());
                    usedAlts.Set(index);
                    if (const auto c = inMulti->GetItem(index)) {
                        argConstraints.front() = c->GetAllConstraints();
                        outFromChildren.Set(i + 1U);
                    }
                }
                status = status.Combine(UpdateLambdaConstraints(input->ChildRef(++i), ctx, argConstraints));
            } else if (inMulti) {                // Check that we can fall to default branch
                for (auto& item: inMulti->GetItems()) {
                    if (!usedAlts.Test(item.first)) {
                        outFromChildren.Set(i);
                        break;
                    }
                }
            }
        }

        if (status != TStatus::Ok) {
            return status;
        }

        if (!inMulti) {
            outFromChildren.Set(0, input->ChildrenSize());
        }

        auto outType = input->GetTypeAnn();
        if (auto t = GetSeqItemType(outType)) {
            outType = t;
        }
        if (outType->GetKind() == ETypeAnnotationKind::Variant && outType->Cast<TVariantExprType>()->GetUnderlyingType()->GetKind() == ETypeAnnotationKind::Tuple) {
            TVector<const TConstraintSet*> outConstraints;
            TVarIndexConstraintNode::TMapType remapItems;
            for (ui32 i = 1; i < input->ChildrenSize(); ++i) {
                if (input->Child(i)->IsAtom()) {
                    ui32 index = FromString<ui32>(input->Child(i)->Content());
                    ++i;
                    if (outFromChildren.Test(i)) {
                        outConstraints.push_back(&input->Child(i)->GetConstraintSet());
                        if (const auto outMulti = input->Child(i)->GetConstraint<TMultiConstraintNode>()) {
                            for (auto& item: outMulti->GetItems()) {
                                remapItems.push_back(std::make_pair(item.first, index));
                            }
                        }
                    }
                } else {
                    if (outFromChildren.Test(i)) {
                        outConstraints.push_back(&input->Child(i)->GetConstraintSet());
                        const auto outMulti = input->Child(i)->GetConstraint<TMultiConstraintNode>();
                        if (outMulti && inMulti) {
                            for (auto& outItem: outMulti->GetItems()) {
                                for (auto& inItem: inMulti->GetItems()) {
                                    if (!usedAlts.Test(inItem.first)) {
                                        remapItems.push_back(std::make_pair(outItem.first, inItem.first));
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (auto multi = TMultiConstraintNode::MakeCommon(outConstraints, ctx)) {
                input->AddConstraint(multi);
            }

            if (auto empty = TEmptyConstraintNode::MakeCommon(outConstraints, ctx)) {
                input->AddConstraint(empty);
            }

            if (auto varIndex = input->Head().GetConstraint<TVarIndexConstraintNode>()) {
                TVarIndexConstraintNode::TMapType varIndexItems;
                for (auto& item: remapItems) {
                    const auto range = varIndex->GetIndexMapping().equal_range(item.second);
                    for (auto i = range.first; i != range.second; ++i) {
                        varIndexItems.push_back(std::make_pair(item.first, i->second));
                    }
                }
                if (!varIndexItems.empty()) {
                    ::Sort(varIndexItems);
                    input->AddConstraint(ctx.MakeConstraint<TVarIndexConstraintNode>(std::move(varIndexItems)));
                }
            }
        }
        else {
            std::vector<const TConstraintSet*> nonEmpty;
            for (ui32 i = 1; i < input->ChildrenSize(); ++i) {
                if (input->Child(i)->IsAtom()) {
                    ++i;
                }
                if (outFromChildren.Test(i)) {
                    nonEmpty.push_back(&input->Child(i)->GetConstraintSet());
                }
            }
            EraseIf(nonEmpty, [] (const TConstraintSet* c) { return !!c->GetConstraint<TEmptyConstraintNode>(); });

            if (nonEmpty.empty()) {
                input->AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
            } else if (nonEmpty.size() == 1) {
                input->SetConstraints(*nonEmpty.front());
            }

            if (auto varIndex = input->Head().GetConstraint<TVarIndexConstraintNode>()) {
                TVarIndexConstraintNode::TMapType varIndexItems;
                for (ui32 i = 1; i < input->ChildrenSize(); ++i) {
                    if (input->Child(i)->IsAtom()) {
                        const auto index = FromString<ui32>(input->Child(i++)->Content());
                        if (outFromChildren.Test(i) && IsDepended(input->Child(i)->Tail(), input->Child(i)->Head().Head())) { // Somehow depends on arg
                            const auto range = varIndex->GetIndexMapping().equal_range(index);
                            for (auto i = range.first; i != range.second; ++i) {
                                varIndexItems.push_back(std::make_pair(0, i->second));
                            }
                        }
                    }
                    else if (outFromChildren.Test(i)) {
                        if (inMulti) {
                            for (auto& inItem: inMulti->GetItems()) {
                                if (!usedAlts.Test(inItem.first)) {
                                    auto range = varIndex->GetIndexMapping().equal_range(inItem.first);
                                    for (auto i = range.first; i != range.second; ++i) {
                                        varIndexItems.push_back(std::make_pair(0, i->second));
                                    }
                                }
                            }
                        }
                    }
                }

                if (!varIndexItems.empty()) {
                    ::SortUnique(varIndexItems);
                    input->AddConstraint(ctx.MakeConstraint<TVarIndexConstraintNode>(std::move(varIndexItems)));
                }
            }

        }
        return TStatus::Ok;
    }

    TStatus VariantItemWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        auto inputType = input->Head().GetTypeAnn();
        if (inputType->GetKind() == ETypeAnnotationKind::Optional) {
            inputType = inputType->Cast<TOptionalExprType>()->GetItemType();
        }

        const auto underlyingType = inputType->Cast<TVariantExprType>()->GetUnderlyingType();
        if (underlyingType->GetKind() == ETypeAnnotationKind::Tuple) {
            if (auto multi = input->Head().GetConstraint<TMultiConstraintNode>()) {
                std::vector<TMultiConstraintNode::TMapType::value_type> nonEmpty;
                std::copy_if(multi->GetItems().begin(), multi->GetItems().end(), std::back_inserter(nonEmpty),
                    [] (const TMultiConstraintNode::TMapType::value_type& v) {
                        return !v.second.GetConstraint<TEmptyConstraintNode>();
                    }
                );

                if (nonEmpty.empty()) {
                    input->AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
                } else if (nonEmpty.size() == 1) {
                    input->SetConstraints(nonEmpty.front().second);
                }
            }
            if (auto varIndex = input->Head().GetConstraint<TVarIndexConstraintNode>()) {
                TVarIndexConstraintNode::TMapType varIndexItems;
                for (auto& item: varIndex->GetIndexMapping()) {
                    varIndexItems.push_back(std::make_pair(0, item.second));
                }
                if (!varIndexItems.empty()) {
                    ::SortUnique(varIndexItems);
                    input->AddConstraint(ctx.MakeConstraint<TVarIndexConstraintNode>(std::move(varIndexItems)));
                }
            }
        }
        return TStatus::Ok;
    }

    TStatus VariantWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        if (input->GetTypeAnn()->Cast<TVariantExprType>()->GetUnderlyingType()->GetKind() == ETypeAnnotationKind::Tuple) {
            const auto index = FromString<ui32>(input->Child(1)->Content());
            TConstraintSet target;
            CopyExcept(target, input->Head().GetConstraintSet(), TVarIndexConstraintNode::Name());
            input->AddConstraint(ctx.MakeConstraint<TMultiConstraintNode>(index, target));
            if (auto varIndex = input->Head().GetConstraint<TVarIndexConstraintNode>()) {
                TVarIndexConstraintNode::TMapType filteredItems;
                for (auto& item: varIndex->GetIndexMapping()) {
                    filteredItems.push_back(std::make_pair(index, item.second));
                }
                input->AddConstraint(ctx.MakeConstraint<TVarIndexConstraintNode>(std::move(filteredItems)));
            }
        }
        return TStatus::Ok;
    }

    TStatus GuessWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        auto inputType = input->Head().GetTypeAnn();
        if (inputType->GetKind() == ETypeAnnotationKind::Optional) {
            inputType = inputType->Cast<TOptionalExprType>()->GetItemType();
        }

        const auto underlyingType = inputType->Cast<TVariantExprType>()->GetUnderlyingType();
        if (underlyingType->GetKind() == ETypeAnnotationKind::Tuple) {
            const auto guessIndex = FromString<ui32>(input->Child(1)->Content());
            if (auto multi = input->Head().GetConstraint<TMultiConstraintNode>()) {
                if (auto c = multi->GetItem(guessIndex)) {
                    input->SetConstraints(*c);
                } else {
                    input->AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
                }
            }
            if (auto varIndex = input->Head().GetConstraint<TVarIndexConstraintNode>()) {
                TVarIndexConstraintNode::TMapType filteredItems;
                for (auto& item: varIndex->GetIndexMapping()) {
                    if (item.first == guessIndex) {
                        filteredItems.push_back(std::make_pair(0, item.second));
                    }
                }
                if (!filteredItems.empty()) {
                    input->AddConstraint(ctx.MakeConstraint<TVarIndexConstraintNode>(std::move(filteredItems)));
                }
            }
        }
        return TStatus::Ok;
    }

    TStatus MuxWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        const auto listItemType = GetSeqItemType(input->GetTypeAnn());
        if (!listItemType) {
            return TStatus::Ok;
        }
        if (listItemType->Cast<TVariantExprType>()->GetUnderlyingType()->GetKind() == ETypeAnnotationKind::Tuple) {
            if (input->Head().IsList()) {
                TMultiConstraintNode::TMapType items;
                ui32 index = 0;
                for (auto& child: input->Head().Children()) {
                    items.push_back(std::make_pair(index, child->GetConstraintSet()));
                    ++index;
                }
                if (!items.empty()) {
                    input->AddConstraint(ctx.MakeConstraint<TMultiConstraintNode>(std::move(items)));
                }
            }
        }
        return TStatus::Ok;
    }

    TStatus NthWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        const auto& memberName = input->Tail().Content();
        const auto& structNode = input->Head();
        if (const auto structPassthrough = structNode.GetConstraint<TPassthroughConstraintNode>()) {
            if (const auto p = structPassthrough->ExtractField(ctx, memberName)) {
                input->AddConstraint(p);
            }
        }
        if (const auto emptyConstraint = structNode.GetConstraint<TEmptyConstraintNode>()) {
            input->AddConstraint(emptyConstraint);
        } else if (const auto part = structNode.GetConstraint<TPartOfUniqueConstraintNode>()) {
            if (const auto extracted = part->ExtractField(ctx, memberName)) {
                input->AddConstraint(extracted);
            }
        }

        if (input->Head().IsList()) {
            input->CopyConstraints(*input->Head().Child(FromString<ui32>(input->Child(1)->Content())));
        }
        else if (input->Head().IsCallable("Demux")) {
            if (auto multi = input->Head().Head().GetConstraint<TMultiConstraintNode>()) {
                if (auto c = multi->GetItem(FromString<ui32>(input->Child(1)->Content()))) {
                    input->SetConstraints(*c);
                }
            }
        }
        return TStatus::Ok;
    }

    TStatus EquiJoinWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        const auto numLists = input->ChildrenSize() - 2U;
        std::vector<size_t> emptyInputs;
        TJoinLabels labels;
        for (auto i = 0U; i < numLists; ++i) {
            const auto& list = input->Child(i)->Head();
            if (list.GetConstraint<TEmptyConstraintNode>()) {
                emptyInputs.push_back(i);
            }
            if (const auto err = labels.Add(ctx, input->Child(i)->Tail(),
                GetSeqItemType(*list.GetTypeAnn()).Cast<TStructExprType>(), GetDetailedUnique(list.GetConstraint<TUniqueConstraintNode>(), *list.GetTypeAnn(), ctx))) {
                ctx.AddError(*err);
                return TStatus::Error;
            }
        }

        const auto joinTree = input->Child(numLists);
        for (auto i: emptyInputs) {
            if (IsRequiredSide(joinTree, labels, i).first) {
                input->AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
                break;
            }
        }

        const TUniqueConstraintNode* unique = nullptr;
        if (const auto status = EquiJoinUniq(input->Pos(), unique, labels, *joinTree, ctx); status != IGraphTransformer::TStatus::Ok) {
            return status;
        }

        if (unique) {
            if (const auto renames = LoadJoinRenameMap(input->Tail()); !renames.empty()) {
                unique = unique->RenameFields(ctx, [&renames](const std::string_view& name) {
                    const auto it = renames.find(name);
                    return renames.cend() == it ? std::vector<std::string_view>() : std::vector<std::string_view>(it->second.cbegin(), it->second.cend());
                });
            }
        }

        if (unique)
            input->AddConstraint(unique);

        return TStatus::Ok;
    }

    TStatus MapJoinCoreWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& /*ctx*/) const {
        if (const auto empty = input->Head().GetConstraint<TEmptyConstraintNode>()) {
            input->AddConstraint(empty);
        } else if (const auto empty = input->Child(1)->GetConstraint<TEmptyConstraintNode>()) {
            if (input->Child(2)->IsAtom({"Inner", "LeftSemi"})) {
                input->AddConstraint(empty);
            }
        }
        return TStatus::Ok;
    }

    TStatus IsKeySwitchWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& /*ctx*/) const {
        if (const auto status = UpdateLambdaConstraints(*input->Child(TCoIsKeySwitch::idx_ItemKeyExtractor))
            .Combine(UpdateLambdaConstraints(*input->Child(TCoIsKeySwitch::idx_StateKeyExtractor))); status != TStatus::Ok) {
            return status;
        }

        if (const auto groupBy = input->Head().GetConstraint<TGroupByConstraintNode>()) {
            TVector<TStringBuf> keys;
            ExtractKeys(*input->Child(2), keys);
            if (!keys.empty()) {
                if (AllOf(keys, [groupBy] (TStringBuf key) { return groupBy->GetColumns().find(key) != groupBy->GetColumns().end(); })) {
                    input->AddConstraint(groupBy);
                }
            }
        }
        return TStatus::Ok;
    }

    TStatus CondenseWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        const auto inputPassthrough = input->Head().GetConstraint<TPassthroughConstraintNode>();
        if (input->Child(1)->IsLambda()) {
            TConstraintNode::TListType argConstraints;
            if (inputPassthrough)
                argConstraints.emplace_back(inputPassthrough);
            if (const auto status = UpdateLambdaConstraints(input->ChildRef(1), ctx, {argConstraints}); status != TStatus::Ok) {
                return status;
            }
        }

        const auto initState = input->Child(1);
        auto stateConstraints = initState->GetAllConstraints();
        stateConstraints.erase(
            std::remove_if(
                stateConstraints.begin(),
                stateConstraints.end(),
                [](const TConstraintNode* c) { return c->GetName() == TEmptyConstraintNode::Name(); }
            ),
            stateConstraints.end()
        );

        TConstraintNode::TListType itemConstraints;
        if (inputPassthrough)
            itemConstraints.emplace_back(inputPassthrough);
        if (const auto groupBy = input->Head().GetConstraint<TGroupByConstraintNode>()) {
            itemConstraints.push_back(groupBy);
        }

        if (const auto status = UpdateLambdaConstraints(input->ChildRef(2), ctx, {itemConstraints, stateConstraints})
            .Combine(UpdateLambdaConstraints(input->TailRef(), ctx, {itemConstraints, stateConstraints})); status != TStatus::Ok) {
            return status;
        }

        const TPassthroughConstraintNode* commonPassthrough = nullptr;
        const auto updateLambda = input->Child(3);
        if (const auto lambdaPassthrough = updateLambda->GetConstraint<TPassthroughConstraintNode>()) {
            if (initState->IsLambda()) {
                if (const auto initPassthrough = initState->GetConstraint<TPassthroughConstraintNode>()) {
                    std::array<TConstraintSet, 2U> set;
                    set.front().AddConstraint(initPassthrough);
                    set.back().AddConstraint(lambdaPassthrough);
                    if (commonPassthrough = TPassthroughConstraintNode::MakeCommon({&set.front(), &set.back()}, ctx))
                        input->AddConstraint(commonPassthrough);
                }
            } else {
                input->AddConstraint(commonPassthrough = lambdaPassthrough);
            }
        }

        if (const auto switchLambda = input->Child(2); switchLambda->Tail().IsCallable(TCoBool::CallableName()) && IsFalse(switchLambda->Tail().Head().Content())) {
            if (const auto& fields = GetAllItemTypeFields(*input->GetTypeAnn(), ctx); !fields.empty()) {
                TUniqueConstraintNode::TFullSetType sets;
                sets.reserve(fields.size());
                for (const auto& field: fields)
                    sets.insert_unique(TUniqueConstraintNode::TSetType{TConstraintNode::TPathType(1U, field)});
                input->AddConstraint(ctx.MakeConstraint<TUniqueConstraintNode>(std::move(sets)));
            }
        }
        else {
            TVector<TStringBuf> groupByKeys;
            if (const auto groupBy = switchLambda->GetConstraint<TGroupByConstraintNode>()) {
                groupByKeys.assign(groupBy->GetColumns().begin(), groupBy->GetColumns().end());
            } else if (switchLambda->Tail().IsCallable({"AggrNotEquals", "NotEquals"})) {
                ExtractSimpleKeys(switchLambda->Child(1)->Child(0), switchLambda->Head().Child(0), groupByKeys);
            }
            if (!groupByKeys.empty() && commonPassthrough) {
                const auto& mapping = commonPassthrough->GetReverseMapping();
                std::vector<std::string_view> uniqColumns;
                for (auto key: groupByKeys) {
                    auto range = mapping.equal_range(key);
                    if (range.first != range.second) {
                        for (auto i = range.first; i != range.second; ++i) {
                            uniqColumns.emplace_back(i->second);
                        }
                    } else {
                        uniqColumns.clear();
                        break;
                    }
                }
                if (!uniqColumns.empty()) {
                    input->AddConstraint(ctx.MakeConstraint<TUniqueConstraintNode>(uniqColumns));
                }
            }
        }

        return FromFirst<TEmptyConstraintNode>(input, output, ctx);
    }

    TStatus WideCondense1Wrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        TSmallVec<TConstraintNode::TListType> argConstraints(input->Child(1)->Head().ChildrenSize());
        const auto inputPassthrough = input->Head().GetConstraint<TPassthroughConstraintNode>();
        const auto groupBy = input->Head().GetConstraint<TGroupByConstraintNode>();
        for (ui32 i = 0U; i < argConstraints.size(); ++i) {
            if (groupBy)
                argConstraints[i].push_back(groupBy);
            if (inputPassthrough)
                if (const auto fieldPasstrought = inputPassthrough->ExtractField(ctx, ctx.GetIndexAsString(i)))
                    argConstraints[i].emplace_back(fieldPasstrought);
        }

        if (const auto status = UpdateLambdaConstraints(input->ChildRef(1), ctx, argConstraints); status != TStatus::Ok) {
            return status;
        }

        const auto initLambda = input->Child(1);
        argConstraints.reserve(argConstraints.size() + initLambda->ChildrenSize() - 1U);
        for (ui32 i = 1U; i < initLambda->ChildrenSize(); ++i) {
            argConstraints.emplace_back(initLambda->Child(i)->GetAllConstraints());
            argConstraints.back().erase(
                std::remove_if(
                    argConstraints.back().begin(),
                    argConstraints.back().end(),
                    [](const TConstraintNode* c) { return c->GetName() == TEmptyConstraintNode::Name(); }
                ),
                argConstraints.back().cend()
            );
        }

        if (const auto status = UpdateLambdaConstraints(input->ChildRef(2), ctx, argConstraints)
            .Combine(UpdateLambdaConstraints(input->TailRef(), ctx, argConstraints)); status != TStatus::Ok) {
            return status;
        }

        const TPassthroughConstraintNode* commonPassthrough = nullptr;
        if (inputPassthrough) {
            const auto updateLambda = input->Child(3);
            const auto initPassthrough = GetConstraintFromLambda<TPassthroughConstraintNode, true>(*initLambda, ctx);
            const auto updatePassthrough = GetConstraintFromLambda<TPassthroughConstraintNode, true>(*updateLambda, ctx);
            if (initPassthrough && updatePassthrough) {
                std::array<TConstraintSet, 2U> set;
                set.front().AddConstraint(initPassthrough);
                set.back().AddConstraint(updatePassthrough);
                if (commonPassthrough = TPassthroughConstraintNode::MakeCommon({&set.front(), &set.back()}, ctx))
                    input->AddConstraint(commonPassthrough);
            }
        }

        if (const auto switchLambda = input->Child(2); switchLambda->Tail().IsCallable(TCoBool::CallableName()) && IsFalse(switchLambda->Tail().Head().Content())) {
            if (const auto width = initLambda->Head().ChildrenSize()) {
                TUniqueConstraintNode::TFullSetType sets;
                sets.reserve(width);
                for (ui32 i = 0U; i < width; ++i)
                    sets.insert_unique(TUniqueConstraintNode::TSetType{TConstraintNode::TPathType(1U, ctx.GetIndexAsString(i))});
                input->AddConstraint(ctx.MakeConstraint<TUniqueConstraintNode>(std::move(sets)));
            }
        } else {
            TVector<TStringBuf> groupByKeys;
            if (const auto groupBy = switchLambda->GetConstraint<TGroupByConstraintNode>()) {
                groupByKeys.assign(groupBy->GetColumns().begin(), groupBy->GetColumns().end());
            } else if (switchLambda->Tail().IsCallable({"AggrNotEquals", "NotEquals"})) {
                ExtractSimpleKeys(&switchLambda->Tail().Head(), &switchLambda->Head().Head(), groupByKeys);
            }
            if (!groupByKeys.empty() && commonPassthrough) {
                auto mapping = commonPassthrough->GetReverseMapping();
                std::vector<std::string_view> uniqColumns;
                for (auto key: groupByKeys) {
                    auto range = mapping.equal_range(key);
                    if (range.first != range.second) {
                        for (auto i = range.first; i != range.second; ++i) {
                            uniqColumns.emplace_back(i->second);
                        }
                    } else {
                        uniqColumns.clear();
                        break;
                    }
                }
                if (!uniqColumns.empty()) {
                    input->AddConstraint(ctx.MakeConstraint<TUniqueConstraintNode>(uniqColumns));
                }
            }
        }

        return FromFirst<TEmptyConstraintNode>(input, output, ctx);
    }

    TStatus GroupByKeyWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        const TStructExprType* inItemType = GetNonEmptyStructItemType(*input->Head().GetTypeAnn());
        const TStructExprType* outItemType = GetNonEmptyStructItemType(*input->GetTypeAnn());
        if (inItemType && outItemType) {
            const auto keySelector = input->Child(TCoGroupByKey::idx_KeySelectorLambda);
            if (const auto status = UpdateLambdaConstraints(*keySelector); status != TStatus::Ok) {
                return status;
            }
            TConstraintNode::TListType argConstraints;
            if (const auto inputPassthrough = input->Head().GetConstraint<TPassthroughConstraintNode>())
                argConstraints.emplace_back(inputPassthrough);
            if (const auto status = UpdateLambdaConstraints(input->ChildRef(TCoGroupByKey::idx_HandlerLambda), ctx, {TConstraintNode::TListType{}, argConstraints}); status != TStatus::Ok) {
                return status;
            }

            if (const auto handlerLambda = input->Child(TCoGroupByKey::idx_HandlerLambda); handlerLambda->GetTypeAnn()->GetKind() == ETypeAnnotationKind::Optional) {
                TVector<TStringBuf> groupKeys;
                ExtractKeys(*keySelector, groupKeys);
                if (!groupKeys.empty()) {
                    if (const auto passthrough = handlerLambda->GetConstraint<TPassthroughConstraintNode>()) {
                        const auto mapping = passthrough->GetReverseMapping();
                        std::vector<std::string_view> uniqColumns;
                        for (auto key: groupKeys) {
                            auto range = mapping.equal_range(key);
                            if (range.first != range.second) {
                                for (auto i = range.first; i != range.second; ++i) {
                                    uniqColumns.emplace_back(i->second);
                                }
                            } else {
                                uniqColumns.clear();
                                break;
                            }
                        }
                        if (!uniqColumns.empty()) {
                            input->AddConstraint(ctx.MakeConstraint<TUniqueConstraintNode>(uniqColumns));
                        }
                    }
                }
            }
        } else {
            auto status = UpdateAllChildLambdasConstraints(*input);
            if (status != TStatus::Ok) {
                return status;
            }
        }

        TApplyConstraintFromInput<TCoGroupByKey::idx_HandlerLambda, TMultiConstraintNode, TEmptyConstraintNode>::Do(input);
        return FromFirst<TEmptyConstraintNode>(input, output, ctx);
    }

    TStatus PartitionByKeyWrap(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) const {
        if (const auto status = UpdateLambdaConstraints(*input->Child(TCoPartitionByKeyBase::idx_KeySelectorLambda)); status != TStatus::Ok) {
            return status;
        }
        if (const auto sortKeySelector = input->Child(TCoPartitionByKeyBase::idx_SortKeySelectorLambda); sortKeySelector->IsLambda()) {
            if (const auto status = UpdateLambdaConstraints(*sortKeySelector); status != TStatus::Ok) {
                return status;
            }
        }

        std::unordered_set<const TPassthroughConstraintNode*> explicitPasstrought;
        auto argConstraints = GetConstraintsForInputArgument(*input, explicitPasstrought, ctx);

        TVector<TStringBuf> partitionKeys;
        ExtractKeys(*input->Child(TCoPartitionByKeyBase::idx_KeySelectorLambda), partitionKeys);
        if (!partitionKeys.empty()) {
            argConstraints.push_back(ctx.MakeConstraint<TGroupByConstraintNode>(std::move(partitionKeys)));
        }

        if (const auto status = UpdateLambdaConstraints(input->ChildRef(TCoPartitionByKeyBase::idx_ListHandlerLambda), ctx, {argConstraints}); status != TStatus::Ok) {
            return status;
        }

        const auto handlerLambda = input->Child(TCoPartitionByKeyBase::idx_ListHandlerLambda);
        const auto lambdaPassthrough = handlerLambda->GetConstraint<TPassthroughConstraintNode>();
        if (lambdaPassthrough) {
            if (!explicitPasstrought.contains(lambdaPassthrough)) {
                auto mapping = lambdaPassthrough->GetColumnMapping();
                for (const auto myPasstrought : explicitPasstrought)
                    mapping.erase(myPasstrought);
                if (!mapping.empty()) {
                    input->AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(mapping)));
                }
            }
        }

        if (!partitionKeys.empty() && lambdaPassthrough) {
            if (auto uniq = handlerLambda->GetConstraint<TUniqueConstraintNode>()) {
                auto mapping = lambdaPassthrough->GetReverseMapping();
                std::vector<std::string_view> uniqColumns;
                for (auto key: partitionKeys) {
                    auto range = mapping.equal_range(key);
                    if (range.first != range.second) {
                        for (auto i = range.first; i != range.second; ++i) {
                            uniqColumns.emplace_back(i->second);
                        }
                    } else {
                        uniqColumns.clear();
                        break;
                    }
                }
                if (uniq->HasEqualColumns(uniqColumns)) {
                    input->AddConstraint(ctx.MakeConstraint<TUniqueConstraintNode>(uniqColumns));
                }
            }
        }

        const bool multiInput = ETypeAnnotationKind::Variant == GetSeqItemType(*input->Head().GetTypeAnn()).GetKind();
        const auto lambdaVarIndex = handlerLambda->GetConstraint<TVarIndexConstraintNode>();
        const auto multi = input->Head().GetConstraint<TMultiConstraintNode>();
        const auto lambdaMulti = handlerLambda->GetConstraint<TMultiConstraintNode>();

        if (const auto varIndex = input->Head().GetConstraint<TVarIndexConstraintNode>()) {
            if (multiInput) {
                if (lambdaVarIndex) {
                    if (auto outVarIndex = GetVarIndexOverVarIndexConstraint(*varIndex, *lambdaVarIndex, ctx)) {
                        input->AddConstraint(outVarIndex);
                    }
                }
            } else {
                if (lambdaMulti) {
                    TVarIndexConstraintNode::TMapType remapItems;
                    for (auto& multiItem: lambdaMulti->GetItems()) {
                        for (auto& varItem: varIndex->GetIndexMapping()) {
                            remapItems.push_back(std::make_pair(multiItem.first, varItem.second));
                        }
                    }
                    if (!remapItems.empty()) {
                        ::SortUnique(remapItems);
                        input->AddConstraint(ctx.MakeConstraint<TVarIndexConstraintNode>(std::move(remapItems)));
                    }
                } else {
                    input->AddConstraint(varIndex);
                }
            }
        }

        if (lambdaMulti && !input->Head().GetConstraint<TEmptyConstraintNode>()) {
            TMultiConstraintNode::TMapType remappedItems;
            for (auto& item: lambdaMulti->GetItems()) {
                remappedItems.push_back(std::make_pair(item.first, TConstraintSet{}));
                if (!multiInput) { // remapping one to many
                    if (const auto lambdaPassthrough = item.second.template GetConstraint<TPassthroughConstraintNode>()) {
                        if (!explicitPasstrought.contains(lambdaPassthrough)) {
                            auto mapping = lambdaPassthrough->GetColumnMapping();
                            for (const auto myPasstrought : explicitPasstrought)
                                mapping.erase(myPasstrought);
                            if (!mapping.empty()) {
                                remappedItems.back().second.AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(mapping)));
                            }
                        }
                        if (const auto lambdaUnique = item.second.template GetConstraint<TUniqueConstraintNode>()) {
                            auto mapping = lambdaPassthrough->GetReverseMapping();
                            std::vector<std::string_view> uniqColumns;
                            for (auto key: partitionKeys) {
                                auto range = mapping.equal_range(key);
                                if (range.first != range.second) {
                                    for (auto i = range.first; i != range.second; ++i) {
                                        uniqColumns.emplace_back(i->second);
                                    }
                                } else {
                                    uniqColumns.clear();
                                    break;
                                }
                            }
                            if (lambdaUnique->HasEqualColumns(uniqColumns)) {
                                remappedItems.back().second.AddConstraint(ctx.MakeConstraint<TUniqueConstraintNode>(uniqColumns));
                            }
                        }
                    }

                    if (const auto empty = item.second.template GetConstraint<TEmptyConstraintNode>()) {
                        remappedItems.pop_back();
                    }
                }
                else if (lambdaVarIndex && multi) {
                    const auto range = lambdaVarIndex->GetIndexMapping().equal_range(item.first);
                    switch (std::distance(range.first, range.second)) {
                    case 0: // new index
                        break;
                    case 1: // remapping 1 to 1
                        if (auto origConstr = multi->GetItem(range.first->second)) {
                            if (const auto lambdaPassthrough = item.second.template GetConstraint<TPassthroughConstraintNode>()) {
                                if (!explicitPasstrought.contains(lambdaPassthrough)) {
                                    auto mapping = lambdaPassthrough->GetColumnMapping();
                                    for (const auto myPasstrought : explicitPasstrought)
                                        mapping.erase(myPasstrought);
                                    if (!mapping.empty()) {
                                        remappedItems.back().second.AddConstraint(ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(mapping)));
                                    }
                                }
                                if (const auto lambdaUnique = item.second.template GetConstraint<TUniqueConstraintNode>()) {
                                    auto mapping = lambdaPassthrough->GetReverseMapping();
                                    std::vector<std::string_view> uniqColumns;
                                    for (auto key: partitionKeys) {
                                        auto range = mapping.equal_range(key);
                                        if (range.first != range.second) {
                                            for (auto i = range.first; i != range.second; ++i) {
                                                uniqColumns.emplace_back(i->second);
                                            }
                                        } else {
                                            uniqColumns.clear();
                                            break;
                                        }
                                    }
                                    if (lambdaUnique->HasEqualColumns(uniqColumns)) {
                                        remappedItems.back().second.AddConstraint(ctx.MakeConstraint<TUniqueConstraintNode>(uniqColumns));
                                    }
                                }
                            }
                            if (const auto empty = item.second.template GetConstraint<TEmptyConstraintNode>()) {
                                remappedItems.pop_back();
                            }
                        } else {
                            remappedItems.pop_back();
                        }
                        break;
                    default: // remapping many to one
                        {
                            std::vector<const TConstraintSet*> nonEmpty;
                            for (auto i = range.first; i != range.second; ++i) {
                                if (auto origConstr = multi->GetItem(i->second)) {
                                    nonEmpty.push_back(origConstr);
                                }
                            }
                            EraseIf(nonEmpty, [] (const TConstraintSet* c) { return !!c->GetConstraint<TEmptyConstraintNode>(); });

                            if (nonEmpty.empty()) {
                                remappedItems.back().second.AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
                            } else if (nonEmpty.size() == 1) {
                                remappedItems.back().second = std::move(*nonEmpty.front());
                            }
                        }
                    }
                } else {
                    remappedItems.back().second = item.second;
                }
            }
            if (remappedItems) {
                input->AddConstraint(ctx.MakeConstraint<TMultiConstraintNode>(std::move(remappedItems)));
            }
        }
        else if (multi && lambdaVarIndex) { // Many to one
            const auto range = lambdaVarIndex->GetIndexMapping().equal_range(0);
            std::vector<const TConstraintSet*> nonEmpty;
            for (auto i = range.first; i != range.second; ++i) {
                if (auto origConstr = multi->GetItem(i->second)) {
                    nonEmpty.push_back(origConstr);
                }
            }
            EraseIf(nonEmpty, [] (const TConstraintSet* c) { return !!c->GetConstraint<TEmptyConstraintNode>(); });

            if (nonEmpty.empty()) {
                input->AddConstraint(ctx.MakeConstraint<TEmptyConstraintNode>());
            } else if (nonEmpty.size() == 1) {
                input->SetConstraints(*nonEmpty.front());
            }
        }

        TApplyConstraintFromInput<TCoPartitionByKeyBase::idx_ListHandlerLambda, TEmptyConstraintNode>::Do(input);
        return FromFirst<TEmptyConstraintNode>(input, output, ctx);
    }

    TStatus AggregateWrap(const TExprNode::TPtr& input, TExprNode::TPtr& /*output*/, TExprContext& ctx) const {
        if (const auto size = input->Child(1)->ChildrenSize()) {
            std::vector<std::string_view> columns;
            columns.reserve(size);
            for (const auto& child: input->Child(1)->Children()) {
                columns.emplace_back(child->Content());
            }
            input->AddConstraint(ctx.MakeConstraint<TUniqueConstraintNode>(columns));
        }
        return TStatus::Ok;
    }

    TStatus FoldWrap(const TExprNode::TPtr& input, TExprNode::TPtr&, TExprContext& ctx) const {
        const TStructExprType* inItemType = GetNonEmptyStructItemType(*input->Head().GetTypeAnn());
        const TStructExprType* outItemType = GetNonEmptyStructItemType(*input->GetTypeAnn());
        if (!inItemType || !outItemType) {
            return UpdateAllChildLambdasConstraints(*input);
        }

        const auto inputPassthrough = input->Head().GetConstraint<TPassthroughConstraintNode>();
        if (input->Child(1)->IsLambda()) {
            TConstraintNode::TListType argConstraints;
            if (inputPassthrough)
                argConstraints.emplace_back(inputPassthrough);
            if (const auto status = UpdateLambdaConstraints(input->ChildRef(1), ctx, {argConstraints}); status != TStatus::Ok) {
                return status;
            }
        }

        const auto initState = input->Child(1);
        auto stateConstraints = initState->GetAllConstraints();
        stateConstraints.erase(
            std::remove_if(
                stateConstraints.begin(),
                stateConstraints.end(),
                [](const TConstraintNode* c) { return c->GetName() == TEmptyConstraintNode::Name(); }
            ),
            stateConstraints.end()
        );

        TConstraintNode::TListType argConstraints;
        if (inputPassthrough)
            argConstraints.emplace_back(inputPassthrough);

        if (const auto status = UpdateLambdaConstraints(input->TailRef(), ctx, {argConstraints, stateConstraints}); status != TStatus::Ok) {
            return status;
        }

        if (const auto lambdaPassthrough = input->Tail().GetConstraint<TPassthroughConstraintNode>()) {
            input->AddConstraint(lambdaPassthrough);
        }

        return TStatus::Ok;
    }
private:
    template <class TConstraintContainer>
    static void CopyExcept(TConstraintContainer& dst, const TConstraintContainer& from, const TSet<TStringBuf>& except) {
        for (auto c: from.GetAllConstraints()) {
            if (!except.contains(c->GetName())) {
                dst.AddConstraint(c);
            }
        }
    }

    template <class TConstraintContainer>
    static void CopyExcept(TConstraintContainer& dst, const TConstraintContainer& from, TStringBuf except) {
        for (auto c: from.GetAllConstraints()) {
            if (c->GetName() != except) {
                dst.AddConstraint(c);
            }
        }
    }

    static void ExtractKeys(const TExprNode& keySelectorLambda, TVector<TStringBuf>& columns) {
        auto arg = keySelectorLambda.Head().Child(0);
        auto body = keySelectorLambda.Child(1);
        if (body->IsCallable("StablePickle")) {
            body = body->Child(0);
        }
        ExtractSimpleKeys(body, arg, columns);
    }

    static std::vector<std::string_view> GetAllItemTypeFields(const TTypeAnnotationNode& type, TExprContext& ctx) {
        std::vector<std::string_view> fields;
        if (const auto itemType = GetSeqItemType(&type)) {
            switch (itemType->GetKind()) {
                case ETypeAnnotationKind::Struct:
                    if (const auto structType = itemType->Cast<TStructExprType>()) {
                        fields.reserve(structType->GetSize());
                        std::transform(structType->GetItems().cbegin(), structType->GetItems().cend(), std::back_inserter(fields), std::bind(&TItemExprType::GetName, std::placeholders::_1));
                    }
                    break;
                case ETypeAnnotationKind::Tuple:
                    if (const auto size = itemType->Cast<TTupleExprType>()->GetSize()) {
                        fields.resize(size);
                        ui32 i = 0U;
                        std::generate(fields.begin(), fields.end(), [&]() { return ctx.GetIndexAsString(i++); });
                    }
                    break;
                default:
                    break;
            }
        }
        return fields;
    }

    static const TUniqueConstraintNode* GetDetailedUnique(const TUniqueConstraintNode* unique,  const TTypeAnnotationNode& type, TExprContext& ctx) {
        if (!unique)
            return nullptr;

        if (const auto& sets = unique->GetAllSets(); sets.size() != 1U || sets.cbegin()->size() != 1U || !sets.cbegin()->cbegin()->empty())
            return unique;

        const auto& columns = GetAllItemTypeFields(type, ctx);
        return columns.empty() ? nullptr : ctx.MakeConstraint<TUniqueConstraintNode>(columns);
    }

    static const TStructExprType* GetNonEmptyStructItemType(const TTypeAnnotationNode& type) {
        const auto itemType = GetSeqItemType(&type);
        if (!itemType || itemType->GetKind() != ETypeAnnotationKind::Struct) {
            return nullptr;
        }
        const TStructExprType* structType = itemType->Cast<TStructExprType>();
        return structType->GetSize() ? structType : nullptr;
    }

    static const TSortedConstraintNode* DeduceSortConstraint(const TExprNode& list, const TExprNode& directions, const TExprNode& keyExtractor, TExprContext& ctx) {
        if (GetSeqItemType(*list.GetTypeAnn()).GetKind() == ETypeAnnotationKind::Struct) {
            if (const auto& columns = ExtractSimpleSortTraits(directions, keyExtractor); !columns.empty()) {
                TSortedConstraintNode::TContainerType content(columns.size());
                std::transform(columns.cbegin(), columns.cend(), content.begin(), [](const std::pair<std::string_view, bool>& item) {
                   return TSortedConstraintNode::TContainerType::value_type({item.first}, item.second);
                });
                return ctx.MakeConstraint<TSortedConstraintNode>(std::move(content));
            }
        }
        return nullptr;
    }

    static const TVarIndexConstraintNode* GetVarIndexOverVarIndexConstraint(const TVarIndexConstraintNode& inputVarIndex,
        const TVarIndexConstraintNode& varIndex, TExprContext& ctx)
    {
        TVarIndexConstraintNode::TMapType result;
        for (auto& item: varIndex.GetIndexMapping()) {
            auto range = inputVarIndex.GetIndexMapping().equal_range(item.second);
            for (auto it = range.first; it != range.second; ++it) {
                result.push_back(std::make_pair(item.first, it->second));
            }
        }
        if (!result.empty()) {
            return ctx.MakeConstraint<TVarIndexConstraintNode>(std::move(result));
        }
        return nullptr;
    }

    static const TSortedConstraintNode* GetPassthroughSortedConstraint(const TSortedConstraintNode& inputSorted,
        const TPassthroughConstraintNode& passthrough, TExprContext& ctx)
    {
        const auto& reverseMapping = passthrough.GetReverseMapping();
        const auto& content = inputSorted.GetContent();
        TSortedConstraintNode::TContainerType filtered;
        for (auto i = 0U; i < content.size(); ++i) {
            TSortedConstraintNode::TContainerType::value_type nextItem;
            for (const auto& column : content[i].first) {
                auto range = reverseMapping.equal_range(column);
                if (range.first != range.second) {
                    for (auto it = range.first; it != range.second; ++it) {
                        nextItem.first.emplace_back(it->second);
                    }
                }
            }
            if (nextItem.first.empty())
                break;

            nextItem.second = content[i].second;
            ::Sort(nextItem.first);
            filtered.emplace_back(std::move(nextItem));
        }

        if (!filtered.empty()) {
            return ctx.MakeConstraint<TSortedConstraintNode>(std::move(filtered));
        }
        return nullptr;
    }

    static const TExprNode* SkipModifiers(const TExprNode* valueNode) {
        if (TCoJust::Match(valueNode)) {
            return SkipModifiers(valueNode->Child(0));
        }
        if (TCoUnwrap::Match(valueNode)) {
            return SkipModifiers(valueNode->Child(0));
        }
        return valueNode;
    }

private:
    const bool SubGraph;
    std::unordered_map<std::string_view, THandler> Functions;
};

template<> const TPassthroughConstraintNode*
TCallableConstraintTransformer::GetConstraintFromWideResultLambda<TPassthroughConstraintNode>(const TExprNode& lambda, TExprContext& ctx) {
    TPassthroughConstraintNode::TMapType passthrough;
    for (auto i = 1U; i < lambda.ChildrenSize(); ++i) {
        if (const auto pass = lambda.Child(i)->GetConstraint<TPassthroughConstraintNode>()) {
            const auto& name = ctx.GetIndexAsString(i - 1U);
            for (const auto& part : pass->GetColumnMapping()) {
                std::transform(part.second.cbegin(), part.second.cend(), std::back_inserter(passthrough[part.first ? part.first : pass]), [&name](TPassthroughConstraintNode::TPartType::value_type item) {
                    item.first.emplace_front(name);
                    return item;
                });
            }
        }
    }

    return passthrough.empty() ? nullptr: ctx.MakeConstraint<TPassthroughConstraintNode>(std::move(passthrough));
}

template<> const TPartOfUniqueConstraintNode*
TCallableConstraintTransformer::GetConstraintFromWideResultLambda<TPartOfUniqueConstraintNode>(const TExprNode& lambda, TExprContext& ctx) {
    TPartOfUniqueConstraintNode::TMapType uniques;

    for (auto i = 1U; i < lambda.ChildrenSize(); ++i) {
        if (const auto part = lambda.Child(i)->GetConstraint<TPartOfUniqueConstraintNode>())
            TPartOfUniqueConstraintNode::UniqueMerge(uniques, part->GetColumnMapping(ctx.GetIndexAsString(i - 1U)));
    }

    return uniques.empty() ? nullptr : ctx.MakeConstraint<TPartOfUniqueConstraintNode>(std::move(uniques));
}

template<> const TVarIndexConstraintNode*
TCallableConstraintTransformer::TCallableConstraintTransformer::GetConstraintFromWideResultLambda<TVarIndexConstraintNode>(const TExprNode& lambda, TExprContext& ctx) {
    TVector<const TConstraintSet*> structConstraints;
    structConstraints.reserve(lambda.ChildrenSize() - 1U);

    for (auto i = 1U; i < lambda.ChildrenSize(); ++i) {
        auto valueNode = lambda.Child(i);
        if (TCoCoalesce::Match(valueNode)) {
            if (valueNode->Head().GetTypeAnn()->GetKind() != ETypeAnnotationKind::Optional || valueNode->ChildrenSize() == 1) {
                valueNode = valueNode->Child(0);
            }
        }
        if (TCoJust::Match(valueNode)) {
            valueNode = valueNode->Child(0);
        }

        if (TCoMember::Match(valueNode) || TCoNth::Match(valueNode)) {
            structConstraints.push_back(&valueNode->Head().GetConstraintSet());
        } else if (valueNode->Type() == TExprNode::Argument) {
            structConstraints.push_back(&valueNode->GetConstraintSet());
        }
    }

    return TVarIndexConstraintNode::MakeCommon(structConstraints, ctx);
}

template<class TConstraint> const TConstraint*
TCallableConstraintTransformer::GetConstraintFromWideResultLambda(const TExprNode&, TExprContext&) { return nullptr; }

class TDefaultCallableConstraintTransformer : public TSyncTransformerBase {
public:
    TStatus DoTransform(TExprNode::TPtr input, TExprNode::TPtr& output, TExprContext& ctx) override {
        Y_UNUSED(output);
        Y_UNUSED(ctx);
        return UpdateAllChildLambdasConstraints(*input);
    }

    void Rewind() final {
    }
};

class TConstraintTransformer : public TGraphTransformerBase {
public:
    TConstraintTransformer(TAutoPtr<IGraphTransformer> callableTransformer, TTypeAnnotationContext& types)
        : CallableTransformer(callableTransformer)
        , Types(types)
    {
    }

    ~TConstraintTransformer() = default;

    TStatus DoTransform(TExprNode::TPtr input, TExprNode::TPtr& output, TExprContext& ctx) final {
        YQL_PROFILE_SCOPE(DEBUG, "ConstraintTransformer::DoTransform");
        output = input;
        auto status = TransformNode(input, output, ctx);
        UpdateStatusIfChanged(status, input, output);

        if (status.Level != TStatus::Error && HasRenames) {
            output = ctx.ReplaceNodes(std::move(output), Processed);
        }

        Processed.clear();
        if (status == TStatus::Ok) {
            Types.ExpectedConstraints.clear();
        }

        HasRenames = false;
        return status;
    }

    NThreading::TFuture<void> DoGetAsyncFuture(const TExprNode& input) final {
        YQL_PROFILE_SCOPE(DEBUG, "ConstraintTransformer::DoGetAsyncFuture");
        Y_UNUSED(input);
        TVector<NThreading::TFuture<void>> futures;
        for (const auto& callable : CallableInputs) {
            futures.push_back(CallableTransformer->GetAsyncFuture(*callable));
        }

        return WaitExceptionOrAll(futures);
    }

    TStatus DoApplyAsyncChanges(TExprNode::TPtr input, TExprNode::TPtr& output, TExprContext& ctx) final {
        YQL_PROFILE_SCOPE(DEBUG, "ConstraintTransformer::DoApplyAsyncChanges");
        output = input;
        TStatus combinedStatus = TStatus::Ok;
        for (const auto& callable : CallableInputs) {
            callable->SetState(TExprNode::EState::ConstrPending);
            TExprNode::TPtr callableOutput;
            auto status = CallableTransformer->ApplyAsyncChanges(callable, callableOutput, ctx);
            Y_VERIFY(callableOutput);
            YQL_ENSURE(status != TStatus::Async);
            YQL_ENSURE(callableOutput == callable);
            combinedStatus = combinedStatus.Combine(status);
            if (status.Level == TStatus::Error) {
                callable->SetState(TExprNode::EState::Error);
            }
        }

        CallableInputs.clear();
        if (combinedStatus.Level == TStatus::Ok) {
            Processed.clear();
        }

        return combinedStatus;
    }

    void Rewind() final {
        CallableTransformer->Rewind();
        CallableInputs.clear();
        Processed.clear();
        HasRenames = false;
        CurrentFunctions = {};
    }

private:
    TStatus TransformNode(const TExprNode::TPtr& start, TExprNode::TPtr& output, TExprContext& ctx) {
        output = start;
        auto processedPair = Processed.emplace(start.Get(), nullptr); // by default node is not changed
        if (!processedPair.second) {
            if (processedPair.first->second) {
                output = processedPair.first->second;
                return TStatus::Repeat;
            }

            switch (start->GetState()) {
            case TExprNode::EState::Initial:
            case TExprNode::EState::TypeInProgress:
            case TExprNode::EState::TypePending:
                return TStatus(TStatus::Repeat, true);
            case TExprNode::EState::TypeComplete:
                break;
            case TExprNode::EState::ConstrInProgress:
                return IGraphTransformer::TStatus::Async;
            case TExprNode::EState::ConstrPending:
                if (start->Type() == TExprNode::Lambda) {
                    if (start->Head().GetState() != TExprNode::EState::ConstrComplete) {
                        return TStatus::Ok;
                    } else if (start->Head().ChildrenSize() == 0) {
                        break;
                    }
                }

                if (start->Type() == TExprNode::Arguments || start->Type() == TExprNode::Argument) {
                    break;
                }

                return TStatus(TStatus::Repeat, true);
            case TExprNode::EState::ConstrComplete:
            case TExprNode::EState::ExecutionInProgress:
            case TExprNode::EState::ExecutionRequired:
            case TExprNode::EState::ExecutionPending:
            case TExprNode::EState::ExecutionComplete:
                return TStatus::Ok;
            case TExprNode::EState::Error:
                return TStatus::Error;
            default:
                YQL_ENSURE(false, "Unknown state");
            }
        }

        auto input = start;
        for (;;) {
            TIssueScopeGuard issueScope(ctx.IssueManager, [this, input, &ctx]() -> TIssuePtr {
                TStringBuilder str;
                str << "At ";
                switch (input->Type()) {
                case TExprNode::Callable:
                    if (!CurrentFunctions.empty() && CurrentFunctions.top().second) {
                        return nullptr;
                    }

                    if (!CurrentFunctions.empty()) {
                        CurrentFunctions.top().second = true;
                    }

                    str << "function: " << input->Content();
                    break;
                case TExprNode::List:
                    if (CurrentFunctions.empty()) {
                        str << "tuple";
                    } else if (!CurrentFunctions.top().second) {
                        CurrentFunctions.top().second = true;
                        str << "function: " << CurrentFunctions.top().first;
                    } else {
                        return nullptr;
                    }
                    break;
                case TExprNode::Lambda:
                    if (CurrentFunctions.empty()) {
                        str << "lambda";
                    } else if (!CurrentFunctions.top().second) {
                        CurrentFunctions.top().second = true;
                        str << "function: " << CurrentFunctions.top().first;
                    } else {
                        return nullptr;
                    }
                    break;
                default:
                    str << "unknown";
                }

                return MakeIntrusive<TIssue>(ctx.GetPosition(input->Pos()), str);
            });

            if (input->IsCallable()) {
                CurrentFunctions.emplace(input->Content(), false);
            }
            Y_SCOPE_EXIT(this, input) {
                if (input->IsCallable()) {
                    CurrentFunctions.pop();
                    if (!CurrentFunctions.empty() && CurrentFunctions.top().first.ends_with('!')) {
                        CurrentFunctions.top().second = true;
                    }
                }
            };

            TStatus retStatus = TStatus::Error;
            switch (input->GetState()) {
            case TExprNode::EState::Initial:
            case TExprNode::EState::TypeInProgress:
            case TExprNode::EState::TypePending:
                return TStatus(TStatus::Repeat, true);
            case TExprNode::EState::TypeComplete:
            case TExprNode::EState::ConstrPending:
                break;
            case TExprNode::EState::ConstrInProgress:
                return IGraphTransformer::TStatus::Async;
            case TExprNode::EState::ConstrComplete:
            case TExprNode::EState::ExecutionInProgress:
            case TExprNode::EState::ExecutionRequired:
            case TExprNode::EState::ExecutionPending:
            case TExprNode::EState::ExecutionComplete:
                return TStatus::Ok;
            case TExprNode::EState::Error:
                return TStatus::Error;
            default:
                YQL_ENSURE(false, "Unknown state");
            }

            input->SetState(TExprNode::EState::ConstrPending);
            switch (input->Type()) {
            case TExprNode::Atom:
            case TExprNode::World:
                input->SetState(TExprNode::EState::ConstrComplete);
                CheckExpected(*input);
                return TStatus::Ok;

            case TExprNode::List:
            {
                retStatus = TransformChildren(input, output, ctx);
                if (retStatus == TStatus::Ok) {
                    retStatus = CallableTransformer->Transform(input, output, ctx);
                    if (retStatus == TStatus::Ok) {
                        input->SetState(TExprNode::EState::ConstrComplete);
                        CheckExpected(*input);
                        break;
                    }
                }

                if (retStatus != TStatus::Error && input != output) {
                    processedPair.first->second = output;
                }
                break;
            }

            case TExprNode::Lambda:
            {
                YQL_ENSURE(input->ChildrenSize() > 0U);
                TExprNode::TPtr out;
                auto argStatus = TransformNode(input->HeadPtr(), out, ctx);
                UpdateStatusIfChanged(argStatus, input->HeadPtr(), out);
                if (argStatus.Level == TStatus::Error) {
                    input->SetState(TExprNode::EState::Error);
                    return argStatus;
                }

                if (argStatus.Level == TStatus::Repeat)
                    return TStatus::Ok;

                TStatus bodyStatus = TStatus::Ok;
                TExprNode::TListType newBody;
                newBody.reserve(input->ChildrenSize() - 1U);
                bool updatedChildren = false;
                for (ui32 i = 1U; i < input->ChildrenSize(); ++i) {
                    const auto child = input->ChildPtr(i);
                    TExprNode::TPtr newChild;
                    auto childStatus = TransformNode(child, newChild, ctx);
                    UpdateStatusIfChanged(childStatus, child, newChild);
                    updatedChildren = updatedChildren || (newChild != child);
                    bodyStatus = bodyStatus.Combine(childStatus);
                    newBody.emplace_back(std::move(newChild));
                }

                retStatus = argStatus.Combine(bodyStatus);
                if (retStatus != TStatus::Ok) {
                    if (retStatus.Level == TStatus::Error) {
                        input->SetState(TExprNode::EState::Error);
                    }
                    else if (updatedChildren) {
                        output = ctx.DeepCopyLambda(*input, std::move(newBody));
                        processedPair.first->second = output;
                        HasRenames = true;
                    }
                } else {
                    if (input->ChildrenSize() != 2U)
                        input->SetState(TExprNode::EState::ConstrComplete);
                    else
                        input->CopyConstraints(input->Tail());
                    CheckExpected(*input);
                }
                break;
            }

            case TExprNode::Argument:
                if (input->GetState() != TExprNode::EState::ConstrComplete) {
                    return TStatus::Repeat;
                }

                return TStatus::Ok;

            case TExprNode::Arguments:
            {
                if (input->Children().empty()) {
                    if (TExprNode::EState::ConstrComplete == input->GetState()) {
                        return TStatus::Ok;
                    }
                    return TStatus::Repeat;
                }

                retStatus = TStatus::Ok;
                for (auto& child : input->Children()) {
                    TExprNode::TPtr tmp;
                    auto childStatus = TransformNode(child, tmp, ctx);
                    UpdateStatusIfChanged(childStatus, child, tmp);
                    YQL_ENSURE(tmp == child);
                    retStatus = retStatus.Combine(childStatus);
                }

                if (retStatus != TStatus::Ok) {
                    if (retStatus.Level == TStatus::Error) {
                        input->SetState(TExprNode::EState::Error);
                    }
                } else {
                    input->SetState(TExprNode::EState::ConstrComplete);
                }
                return retStatus;
            }

            case TExprNode::Callable:
            {
                retStatus = TransformChildren(input, output, ctx);
                if (retStatus != TStatus::Ok) {
                    if (retStatus != TStatus::Error && input != output) {
                        processedPair.first->second = output;
                    }
                    break;
                }

                CurrentFunctions.top().second = true;
                retStatus = CallableTransformer->Transform(input, output, ctx);
                if (retStatus == TStatus::Error) {
                    input->SetState(TExprNode::EState::Error);
                } else if (retStatus == TStatus::Ok) {
                    // Sanity check
                    for (size_t i = 0; i < input->ChildrenSize(); ++i) {
                        YQL_ENSURE(input->Child(i)->GetState() >= TExprNode::EState::ConstrComplete,
                            "Child with index " << i << " of callable " << TString{input->Content()}.Quote() << " has bad state after constraint transform");
                    }
                    input->SetState(TExprNode::EState::ConstrComplete);
                    CheckExpected(*input);
                } else if (retStatus == TStatus::Async) {
                    CallableInputs.push_back(input);
                    input->SetState(TExprNode::EState::ConstrInProgress);
                } else {
                    if (output != input.Get()) {
                        processedPair.first->second = output;
                        HasRenames = true;
                    }
                }
                break;
            }
            default:
                YQL_ENSURE(false, "Unknown type");
            }

            if (retStatus.Level != TStatus::Repeat || retStatus.HasRestart) {
                return retStatus;
            }

            input = output;
        }
    }

    TStatus TransformChildren(const TExprNode::TPtr& input, TExprNode::TPtr& output, TExprContext& ctx) {
        TStatus combinedStatus = TStatus::Ok;
        TExprNode::TListType newChildren;
        bool updatedChildren = false;
        for (ui32 i = 0; i < input->ChildrenSize(); ++i) {
            const auto child = input->ChildPtr(i);
            TExprNode::TPtr newChild;
            auto childStatus = TransformNode(child, newChild, ctx);
            UpdateStatusIfChanged(childStatus, child, newChild);
            updatedChildren = updatedChildren || (newChild != child);
            combinedStatus = combinedStatus.Combine(childStatus);
            newChildren.emplace_back(std::move(newChild));
        }

        if (combinedStatus != TStatus::Ok) {
            if (combinedStatus.Level == TStatus::Error) {
                input->SetState(TExprNode::EState::Error);
            }
            else if (updatedChildren) {
                output = ctx.ChangeChildren(*input, std::move(newChildren));
                HasRenames = true;
            }
        }
        return combinedStatus;
    }

    void UpdateStatusIfChanged(TStatus& status, const TExprNode::TPtr& input, const TExprNode::TPtr& output) {
        if (status.Level == TStatus::Ok && input != output) {
            status = TStatus(TStatus::Repeat, status.HasRestart);
        }
    }

    void CheckExpected(const TExprNode& input) {
        if (const auto it = Types.ExpectedConstraints.find(input.UniqueId()); it != Types.ExpectedConstraints.cend()) {
            for (const TConstraintNode* expectedConstr: it->second) {
                if (!Types.DisableConstraintCheck.contains(expectedConstr->GetName())) {
                    auto newConstr = input.GetConstraint(expectedConstr->GetName());
                    if (!newConstr) {
                        if (expectedConstr->GetName() == TMultiConstraintNode::Name()
                            || expectedConstr->GetName() == TPassthroughConstraintNode::Name()) {
                            // Constraint Multi(0:{Empty},1:{Empty}, ..., N:{Empty}) can be reduced to Empty
                            // Constraint Passthrough can be reduced in empty containers
                            newConstr = input.GetConstraint<TEmptyConstraintNode>();
                        }
                        YQL_ENSURE(newConstr, "Rewrite error, missing " << *expectedConstr << " constraint in node " << input.Content());
                    } else {
                        if (expectedConstr->GetName() == TMultiConstraintNode::Name()) {
                            YQL_ENSURE(static_cast<const TMultiConstraintNode*>(newConstr)->FilteredIncludes(*expectedConstr, Types.DisableConstraintCheck), "Rewrite error, unequal " << *newConstr
                                << " constraint in node " << input.Content() << ", previous was " << *expectedConstr);
                        } else {
                            YQL_ENSURE(newConstr->Includes(*expectedConstr), "Rewrite error, unequal " << *newConstr
                                << " constraint in node " << input.Content() << ", previous was " << *expectedConstr);
                        }
                    }
                }
            }
        }
    }

private:
    TAutoPtr<IGraphTransformer> CallableTransformer;
    std::deque<TExprNode::TPtr> CallableInputs;
    TNodeOnNodeOwnedMap Processed;
    bool HasRenames = false;
    std::stack<std::pair<std::string_view, bool>> CurrentFunctions;
    TTypeAnnotationContext& Types;
};

} // namespace

TAutoPtr<IGraphTransformer> CreateConstraintTransformer(TTypeAnnotationContext& types, bool instantOnly, bool subGraph) {
    TAutoPtr<IGraphTransformer> callableTransformer(new TCallableConstraintTransformer(types, instantOnly, subGraph));
    return new TConstraintTransformer(callableTransformer, types);
}

TAutoPtr<IGraphTransformer> CreateDefCallableConstraintTransformer() {
    return new TDefaultCallableConstraintTransformer();
}

IGraphTransformer::TStatus UpdateLambdaConstraints(const TExprNode& lambda) {
    const auto args = lambda.Child(0);
    for (const auto& arg: args->Children()) {
        if (arg->GetState() == TExprNode::EState::TypeComplete || arg->GetState() == TExprNode::EState::ConstrPending) {
            arg->SetState(TExprNode::EState::ConstrComplete);
        }
        YQL_ENSURE(arg->GetAllConstraints().empty());
    }

    if (args->GetState() == TExprNode::EState::TypeComplete || args->GetState() == TExprNode::EState::ConstrPending) {
        args->SetState(TExprNode::EState::ConstrComplete);
    }

    if (lambda.GetState() != TExprNode::EState::ConstrComplete) {
        return IGraphTransformer::TStatus::Repeat;
    }

    return IGraphTransformer::TStatus::Ok;
}

IGraphTransformer::TStatus UpdateLambdaConstraints(TExprNode::TPtr& lambda, TExprContext& ctx, const TArrayRef<const TConstraintNode::TListType>& constraints) {
    bool updateArgs = false;
    const auto args = lambda->Child(0);

    YQL_ENSURE(args->ChildrenSize() == constraints.size());
    size_t i = 0;
    for (const auto& constrList: constraints) {
        const auto arg = args->Child(i++);
        if (arg->GetState() == TExprNode::EState::TypeComplete || arg->GetState() == TExprNode::EState::ConstrPending) {
            for (const auto c: constrList) {
                arg->AddConstraint(c);
            }
            arg->SetState(TExprNode::EState::ConstrComplete);
        } else {
            if (constrList.size() != arg->GetAllConstraints().size() || !AllOf(constrList, [arg] (const TConstraintNode* c) { return arg->GetConstraint(c->GetName()) == c; })) {
                updateArgs = true;
            }
        }
    }

    if (updateArgs) {
        TNodeOnNodeOwnedMap replaces(constraints.size());
        TExprNode::TListType argsChildren;
        argsChildren.reserve(constraints.size());
        i = 0;
        for (const auto& constrList: constraints) {
            const auto arg = args->Child(i++);
            const auto newArg = ctx.ShallowCopy(*arg);
            newArg->SetTypeAnn(arg->GetTypeAnn());
            for (const auto c: constrList) {
                newArg->AddConstraint(c);
            }
            newArg->SetState(TExprNode::EState::ConstrComplete);
            YQL_ENSURE(replaces.emplace(arg, newArg).second);
            argsChildren.emplace_back(std::move(newArg));
        }

        auto newArgs = ctx.NewArguments(args->Pos(), std::move(argsChildren));
        newArgs->SetTypeAnn(ctx.MakeType<TUnitExprType>());
        newArgs->SetState(TExprNode::EState::ConstrComplete);
        const auto type = lambda->GetTypeAnn();
        lambda = ctx.NewLambda(lambda->Pos(), std::move(newArgs), ctx.ReplaceNodes<true>(GetLambdaBody(*lambda), replaces));
        lambda->SetTypeAnn(type);
        lambda->Head().ForEachChild(std::bind(&TExprNode::SetDependencyScope, std::placeholders::_1, lambda.Get(), lambda.Get()));
        return IGraphTransformer::TStatus::Repeat;
    }

    if (args->GetState() == TExprNode::EState::TypeComplete || args->GetState() == TExprNode::EState::ConstrPending) {
        args->SetState(TExprNode::EState::ConstrComplete);
    }

    if (lambda->GetState() != TExprNode::EState::ConstrComplete) {
        return IGraphTransformer::TStatus::Repeat;
    }

    return IGraphTransformer::TStatus::Ok;
}

IGraphTransformer::TStatus UpdateAllChildLambdasConstraints(const TExprNode& node) {
    IGraphTransformer::TStatus status = IGraphTransformer::TStatus::Ok;
    for (ui32 i = 0; i < node.ChildrenSize(); ++i) {
        const auto child = node.Child(i);
        if (child->Type() == TExprNode::EType::Lambda) {
            status = status.Combine(UpdateLambdaConstraints(*child));
        }
    }
    return status;
}

} // namespace NYql
