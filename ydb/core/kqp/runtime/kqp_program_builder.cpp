#include "kqp_program_builder.h"

#include <ydb/core/kqp/common/kqp_yql.h>
#include <ydb/core/scheme/scheme_tabledefs.h>

#include <ydb/library/yql/minikql/mkql_node_cast.h>
#include <ydb/library/yql/minikql/mkql_runtime_version.h>

namespace NKikimr {
namespace NMiniKQL {

namespace {

TType* GetRowType(const TProgramBuilder& builder, const TArrayRef<TKqpTableColumn>& columns) {
    TStructTypeBuilder rowTypeBuilder(builder.GetTypeEnvironment());
    for (auto& column : columns) {
        TType* type = nullptr;
        switch (column.Type) {
            case NUdf::TDataType<NUdf::TDecimal>::Id: {
                type = TDataDecimalType::Create(
                    NScheme::DECIMAL_PRECISION,
                    NScheme::DECIMAL_SCALE,
                    builder.GetTypeEnvironment()
                );
                break;
            }
            case NKikimr::NScheme::NTypeIds::Pg: {
                Y_VERIFY(column.TypeDesc, "No pg type description");
                Y_VERIFY(!column.NotNull, "pg not null types are not allowed");
                type = TPgType::Create(NPg::PgTypeIdFromTypeDesc(column.TypeDesc), builder.GetTypeEnvironment());
                break;
            }
            default: {
                type = TDataType::Create(column.Type, builder.GetTypeEnvironment());
                break;
            }
        }

        if (!column.NotNull && column.Type != NKikimr::NScheme::NTypeIds::Pg) {
            type = TOptionalType::Create(type, builder.GetTypeEnvironment());
        }

        rowTypeBuilder.Add(column.Name, type);
    }

    return rowTypeBuilder.Build();
}

TRuntimeNode BuildColumnTags(const TProgramBuilder& builder, const TArrayRef<TKqpTableColumn>& columns) {
    TStructLiteralBuilder tagsBuilder(builder.GetTypeEnvironment());
    for (auto& column : columns) {
        tagsBuilder.Add(column.Name, builder.NewDataLiteral<ui32>(column.Id));
    }

    return TRuntimeNode(tagsBuilder.Build(), true);
}

TRuntimeNode BuildColumnIndicesMap(const TProgramBuilder& builder, const TStructType& rowType,
    const TArrayRef<TKqpTableColumn>& columns)
{
    TDictLiteralBuilder indicesMap(builder.GetTypeEnvironment(),
        TDataType::Create(NUdf::TDataType<ui32>::Id, builder.GetTypeEnvironment()),
        TDataType::Create(NUdf::TDataType<ui32>::Id, builder.GetTypeEnvironment()));

    for (auto& column : columns) {
        ui32 index = rowType.GetMemberIndex(column.Name);
        indicesMap.Add(builder.NewDataLiteral<ui32>(column.Id), builder.NewDataLiteral<ui32>(index));
    }

    return TRuntimeNode(indicesMap.Build(), true);
}

TRuntimeNode BuildKeyPrefixIndicesList(const TProgramBuilder& builder, const TStructType& rowType,
    const TArrayRef<TKqpTableColumn>& keyColumns)
{
    TListLiteralBuilder indicesList(builder.GetTypeEnvironment(),
        TDataType::Create(NUdf::TDataType<ui32>::Id, builder.GetTypeEnvironment()));

    MKQL_ENSURE_S(rowType.GetMembersCount() <= keyColumns.size());
    for (ui32 i = 0; i < rowType.GetMembersCount(); ++i) {
        auto& keyColumn = keyColumns[i];
        ui32 index = rowType.GetMemberIndex(keyColumn.Name);
        indicesList.Add(builder.NewDataLiteral<ui32>(index));
    }

    return TRuntimeNode(indicesList.Build(), true);
}

TRuntimeNode BuildTableIdLiteral(const TTableId& tableId, TProgramBuilder& builder) {
    TVector<TRuntimeNode> tupleItems {
        builder.NewDataLiteral<ui64>(tableId.PathId.OwnerId),
        builder.NewDataLiteral<ui64>(tableId.PathId.LocalPathId),
        builder.NewDataLiteral<NUdf::EDataSlot::String>(tableId.SysViewInfo),
        builder.NewDataLiteral<ui64>(tableId.SchemaVersion),
    };

    return builder.NewTuple(tupleItems);
}

TRuntimeNode BuildKeyRangeNode(TProgramBuilder& builder, const TKqpKeyRange& range) {
    TVector<TRuntimeNode> rangeItems;
    rangeItems.reserve(4);
    rangeItems.push_back(builder.NewTuple(range.FromTuple));
    rangeItems.push_back(builder.NewDataLiteral(range.FromInclusive));
    rangeItems.push_back(builder.NewTuple(range.ToTuple));
    rangeItems.push_back(builder.NewDataLiteral(range.ToInclusive));

    return builder.NewTuple(rangeItems);
}

TRuntimeNode BuildKeyRangesNode(TProgramBuilder& builder, const TKqpKeyRanges& range) {
    TVector<TRuntimeNode> rangeItems{range.Ranges};
    return builder.NewTuple(rangeItems);
}

TRuntimeNode BuildSkipNullKeysNode(TProgramBuilder& builder, const TKqpKeyRange& range) {
    TListLiteralBuilder skipNullKeysBuilder(
        builder.GetTypeEnvironment(),
        builder.NewDataType(NUdf::TDataType<bool>::Id));

    for (bool skipNull : range.SkipNullKeys) {
        skipNullKeysBuilder.Add(builder.NewDataLiteral(skipNull));
    }
    return TRuntimeNode(skipNullKeysBuilder.Build(), true);
}

TType* MakeWideFlowType(TProgramBuilder& builder, TStructType* rowType) {
    std::vector<TType*> tupleItems;
    tupleItems.reserve(rowType->GetMembersCount());
    for (ui32 i = 0; i < rowType->GetMembersCount(); ++i) {
        tupleItems.push_back(rowType->GetMemberType(i));
    }

    return builder.NewFlowType(builder.NewTupleType(tupleItems));
}

} // namespace

TKqpProgramBuilder::TKqpProgramBuilder(const TTypeEnvironment& env, const IFunctionRegistry& functionRegistry)
    : TProgramBuilder(env, functionRegistry) {}

TRuntimeNode TKqpProgramBuilder::KqpReadTable(const TTableId& tableId, const TKqpKeyRange& range,
    const TArrayRef<TKqpTableColumn>& columns)
{
    auto rowType = GetRowType(*this, columns);
    auto returnType = NewFlowType(rowType);

    TCallableBuilder builder(Env, __func__, returnType);
    builder.Add(BuildTableIdLiteral(tableId, *this));
    builder.Add(BuildKeyRangeNode(*this, range));
    builder.Add(BuildColumnTags(*this, columns));
    builder.Add(BuildSkipNullKeysNode(*this, range));
    builder.Add(range.ItemsLimit ? range.ItemsLimit : NewNull());
    builder.Add(NewDataLiteral(range.Reverse));

    return TRuntimeNode(builder.Build(), false);
}

TRuntimeNode TKqpProgramBuilder::KqpWideReadTable(const TTableId& tableId, const TKqpKeyRange& range,
    const TArrayRef<TKqpTableColumn>& columns)
{
    auto rowType = GetRowType(*this, columns);
    auto structType = AS_TYPE(TStructType, rowType);
    auto returnType = MakeWideFlowType(*this, structType);

    MKQL_ENSURE_S(returnType);
    MKQL_ENSURE_S(returnType->IsFlow());
    const auto itemType = AS_TYPE(TFlowType, returnType)->GetItemType();
    MKQL_ENSURE_S(itemType->IsTuple());

    TCallableBuilder builder(Env, __func__, returnType);
    builder.Add(BuildTableIdLiteral(tableId, *this));
    builder.Add(BuildKeyRangeNode(*this, range));
    builder.Add(BuildColumnTags(*this, columns));
    builder.Add(BuildSkipNullKeysNode(*this, range));
    builder.Add(range.ItemsLimit ? range.ItemsLimit : NewNull());
    builder.Add(NewDataLiteral(range.Reverse));

    return TRuntimeNode(builder.Build(), false);
}

TRuntimeNode TKqpProgramBuilder::KqpWideReadTableRanges(const TTableId& tableId, const TKqpKeyRanges& ranges,
    const TArrayRef<TKqpTableColumn>& columns, TType* returnType)
{
    if (returnType == nullptr) {
        auto rowType = GetRowType(*this, columns);
        auto structType = AS_TYPE(TStructType, rowType);
        returnType = MakeWideFlowType(*this, structType);
    } else {
        MKQL_ENSURE_S(returnType);
        MKQL_ENSURE_S(returnType->IsFlow());
        const auto itemType = AS_TYPE(TFlowType, returnType)->GetItemType();
        MKQL_ENSURE_S(itemType->IsTuple());
    }

    TCallableBuilder builder(Env, __func__, returnType);
    builder.Add(BuildTableIdLiteral(tableId, *this));
    builder.Add(BuildKeyRangesNode(*this, ranges));
    builder.Add(BuildColumnTags(*this, columns));
    builder.Add(ranges.ItemsLimit);
    builder.Add(NewDataLiteral(ranges.Reverse));

    return TRuntimeNode(builder.Build(), false);
}

TRuntimeNode TKqpProgramBuilder::KqpLookupTable(const TTableId& tableId, const TRuntimeNode& lookupKeys,
    const TArrayRef<TKqpTableColumn>& keyColumns, const TArrayRef<TKqpTableColumn>& columns)
{
    auto keysType = AS_TYPE(TStreamType, lookupKeys.GetStaticType());
    auto keyType = AS_TYPE(TStructType, keysType->GetItemType());

    auto rowType = GetRowType(*this, columns);
    auto returnType = NewFlowType(rowType);

    TCallableBuilder builder(Env, __func__, returnType);
    builder.Add(BuildTableIdLiteral(tableId, *this));
    builder.Add(lookupKeys);
    builder.Add(BuildKeyPrefixIndicesList(*this, *keyType, keyColumns));
    builder.Add(BuildColumnTags(*this, columns));

    return TRuntimeNode(builder.Build(), false);
}

TRuntimeNode TKqpProgramBuilder::KqpUpsertRows(const TTableId& tableId, const TRuntimeNode& rows,
    const TArrayRef<TKqpTableColumn>& upsertColumns)
{
    auto streamType = AS_TYPE(TStreamType, rows.GetStaticType());
    auto rowType = AS_TYPE(TStructType, streamType->GetItemType());

    auto returnType = NewStreamType(NewResourceType(NYql::KqpEffectTag));

    TCallableBuilder builder(Env, __func__, returnType);
    builder.Add(BuildTableIdLiteral(tableId, *this));
    builder.Add(rows);
    builder.Add(BuildColumnIndicesMap(*this, *rowType, upsertColumns));

    return TRuntimeNode(builder.Build(), false);
}

TRuntimeNode TKqpProgramBuilder::KqpDeleteRows(const TTableId& tableId, const TRuntimeNode& rows) {
    auto returnType = NewStreamType(NewResourceType(NYql::KqpEffectTag));

    TCallableBuilder builder(Env, __func__, returnType);
    builder.Add(BuildTableIdLiteral(tableId, *this));
    builder.Add(rows);

    return TRuntimeNode(builder.Build(), false);
}

TRuntimeNode TKqpProgramBuilder::KqpEffects(const TArrayRef<const TRuntimeNode>& effects) {
    auto returnType = NewStreamType(NewResourceType(NYql::KqpEffectTag));
    TCallableBuilder builder(Env, __func__, returnType);
    for (auto& effect : effects) {
        builder.Add(effect);
    }

    return TRuntimeNode(builder.Build(), false);
}

TRuntimeNode TKqpProgramBuilder::KqpEnsure(TRuntimeNode value, TRuntimeNode predicate, TRuntimeNode issueCode,
    TRuntimeNode message)
{
    bool isOptional;
    const auto unpackedType = UnpackOptionalData(predicate, isOptional);
    MKQL_ENSURE(unpackedType->GetSchemeType() == NUdf::TDataType<bool>::Id, "Expected bool.");

    const auto& issueCodeType = issueCode.GetStaticType();
    MKQL_ENSURE(issueCodeType->IsData(), "Expected data.");

    const auto& issueCodeTypeData = static_cast<const TDataType&>(*issueCodeType);
    MKQL_ENSURE(issueCodeTypeData.GetSchemeType() == NUdf::TDataType<ui32>::Id, "Expected uint32.");

    const auto& messageType = message.GetStaticType();
    MKQL_ENSURE(messageType->IsData(), "Expected data.");

    const auto& messageTypeData = static_cast<const TDataType&>(*messageType);
    MKQL_ENSURE(messageTypeData.GetSchemeType() == NUdf::TDataType<NUdf::TUtf8>::Id, "Expected string or utf8.");

    TCallableBuilder callableBuilder(Env, __func__, value.GetStaticType());
    callableBuilder.Add(value);
    callableBuilder.Add(predicate);
    callableBuilder.Add(issueCode);
    callableBuilder.Add(message);
    return TRuntimeNode(callableBuilder.Build(), false);
}

} // namespace NMiniKQL
} // namespace NKikimr
