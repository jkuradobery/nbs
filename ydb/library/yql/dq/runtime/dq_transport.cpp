#include "dq_transport.h"
#include "dq_arrow_helpers.h"

#include <ydb/library/mkql_proto/mkql_proto.h>
#include <ydb/library/yql/minikql/computation/mkql_computation_node_holders.h>
#include <ydb/library/yql/minikql/computation/mkql_computation_node_pack.h>
#include <ydb/library/yql/parser/pg_wrapper/interface/comp_factory.h>
#include <ydb/library/yql/parser/pg_wrapper/interface/pack.h>
#include <ydb/library/yql/providers/common/mkql/yql_type_mkql.h>
#include <ydb/library/yql/utils/yql_panic.h>

#include <util/system/yassert.h>

namespace NYql::NDq {

using namespace NKikimr;
using namespace NMiniKQL;
using namespace NYql;

namespace {

NDqProto::TData SerializeBufferArrowV1(TUnboxedValueVector& buffer, const TType* itemType);

void DeserializeBufferArrowV1(const NDqProto::TData& data, const TType* itemType,
                              const THolderFactory& holderFactory, TUnboxedValueVector& buffer);

NDqProto::TData SerializeValuePickleV1(const TType* type, const NUdf::TUnboxedValuePod& value) {
    TValuePacker packer(/* stable */ false, type);
    TStringBuf packResult = packer.Pack(value);

    NDqProto::TData data;
    data.SetTransportVersion(NDqProto::DATA_TRANSPORT_UV_PICKLE_1_0);
    data.SetRaw(packResult.data(), packResult.size());
    data.SetRows(1);

    return data;
}

NDqProto::TData SerializeValueArrowV1(const TType* type, const NUdf::TUnboxedValuePod& value) {
    TUnboxedValueVector buffer;
    buffer.push_back(value);
    return SerializeBufferArrowV1(buffer, type);
}

void DeserializeValuePickleV1(const TType* type, const NDqProto::TData& data, NUdf::TUnboxedValue& value,
    const THolderFactory& holderFactory)
{
    YQL_ENSURE(data.GetTransportVersion() == (ui32) NDqProto::DATA_TRANSPORT_UV_PICKLE_1_0);
    TValuePacker packer(/* stable */ false, type);
    value = packer.Unpack(data.GetRaw(), holderFactory);
}

void DeserializeValueArrowV1(const TType* type, const NDqProto::TData& data, NUdf::TUnboxedValue& value,
    const THolderFactory& holderFactory)
{
    TUnboxedValueVector buffer;
    DeserializeBufferArrowV1(data, type, holderFactory, buffer);
    value = buffer[0];
}

NDqProto::TData SerializeBufferPickleV1(TUnboxedValueVector& buffer, const TType* itemType,
    const TTypeEnvironment& typeEnv, const THolderFactory& holderFactory)
{
    const auto listType = TListType::Create(const_cast<TType*>(itemType), typeEnv);
    const NUdf::TUnboxedValue listValue = holderFactory.VectorAsArray(buffer);

    auto data = SerializeValuePickleV1(listType, listValue);
    data.SetRows(buffer.size());
    return data;
}

NDqProto::TData SerializeBufferArrowV1(TUnboxedValueVector& buffer, const TType* itemType) {
    auto array = NArrow::MakeArray(buffer, itemType);

    auto serialized = NArrow::SerializeArray(array);

    NDqProto::TData data;
    data.SetTransportVersion(NDqProto::DATA_TRANSPORT_ARROW_1_0);
    data.SetRaw(serialized.data(), serialized.size());
    data.SetRows(buffer.size());
    return data;
}

void DeserializeBufferPickleV1(const NDqProto::TData& data, const TType* itemType, const TTypeEnvironment& typeEnv,
    const THolderFactory& holderFactory, TUnboxedValueVector& buffer)
{
    auto listType = TListType::Create(const_cast<TType*>(itemType), typeEnv);

    NUdf::TUnboxedValue value;
    DeserializeValuePickleV1(listType, data, value, holderFactory);

    const auto iter = value.GetListIterator();
    for (NUdf::TUnboxedValue item; iter.Next(item);) {
        buffer.emplace_back(std::move(item));
    }
}

void DeserializeBufferArrowV1(const NDqProto::TData& data, const TType* itemType, const THolderFactory& holderFactory,
    TUnboxedValueVector& buffer)
{
    YQL_ENSURE(data.GetTransportVersion() == (ui32) NDqProto::DATA_TRANSPORT_ARROW_1_0);

    auto array = NArrow::DeserializeArray(data.GetRaw(), NArrow::GetArrowType(itemType));
    YQL_ENSURE(array->length() == data.GetRows());
    auto newElements = NArrow::ExtractUnboxedValues(array, itemType, holderFactory);
    for (NUdf::TUnboxedValue item: newElements) {
        buffer.emplace_back(std::move(item));
    }
}

void DeserializeParamV1(const NDqProto::TData& data, const TType* type, const THolderFactory& holderFactory,
    NUdf::TUnboxedValue& value)
{
    DeserializeValuePickleV1(type, data, value, holderFactory);
}

} // namespace

NDqProto::EDataTransportVersion TDqDataSerializer::GetTransportVersion() const {
    return TransportVersion;
}

NDqProto::TData TDqDataSerializer::Serialize(const NUdf::TUnboxedValue& value, const TType* itemType) const {
    switch (TransportVersion) {
        case NDqProto::DATA_TRANSPORT_VERSION_UNSPECIFIED:
        case NDqProto::DATA_TRANSPORT_UV_PICKLE_1_0:
            return SerializeValuePickleV1(itemType, value);
        case NDqProto::DATA_TRANSPORT_ARROW_1_0:
            return SerializeValueArrowV1(itemType, value);
        default:
            YQL_ENSURE(false, "Unsupported TransportVersion");
    }
}

NDqProto::TData TDqDataSerializer::Serialize(TUnboxedValueVector& buffer, const TType* itemType) const {
    switch (TransportVersion) {
        case NDqProto::DATA_TRANSPORT_VERSION_UNSPECIFIED:
        case NDqProto::DATA_TRANSPORT_UV_PICKLE_1_0:
            return SerializeBufferPickleV1(buffer, itemType, TypeEnv, HolderFactory);
        case NDqProto::DATA_TRANSPORT_ARROW_1_0:
            return SerializeBufferArrowV1(buffer, itemType);
        default:
            YQL_ENSURE(false, "Unsupported TransportVersion");
    }
}

void TDqDataSerializer::Deserialize(const NDqProto::TData& data, const TType* itemType,
    TUnboxedValueVector& buffer) const
{
    switch (TransportVersion) {
        case NDqProto::DATA_TRANSPORT_VERSION_UNSPECIFIED:
        case NDqProto::DATA_TRANSPORT_UV_PICKLE_1_0: {
            DeserializeBufferPickleV1(data, itemType, TypeEnv, HolderFactory, buffer);
            break;
        }
        case NDqProto::DATA_TRANSPORT_ARROW_1_0: {
            DeserializeBufferArrowV1(data, itemType, HolderFactory, buffer);
            break;
        }
        default:
            YQL_ENSURE(false, "Unsupported TransportVersion");
    }
}

void TDqDataSerializer::Deserialize(const NDqProto::TData& data, const TType* itemType,
    NUdf::TUnboxedValue& value) const
{
    switch (TransportVersion) {
        case NDqProto::DATA_TRANSPORT_VERSION_UNSPECIFIED:
        case NDqProto::DATA_TRANSPORT_UV_PICKLE_1_0: {
            DeserializeValuePickleV1(itemType, data, value, HolderFactory);
            break;
        }
        case NDqProto::DATA_TRANSPORT_ARROW_1_0: {
            DeserializeValueArrowV1(itemType, data, value, HolderFactory);
            break;
        }
        default:
            YQL_ENSURE(false, "Unsupported TransportVersion");
    }
}

void TDqDataSerializer::DeserializeParam(const NDqProto::TData& data, const TType* type,
    const NKikimr::NMiniKQL::THolderFactory& holderFactory, NUdf::TUnboxedValue& value)
{
    YQL_ENSURE(data.GetTransportVersion() == (ui32) NDqProto::DATA_TRANSPORT_UV_PICKLE_1_0);

    return DeserializeParamV1(data, type, holderFactory, value);
}

NDqProto::TData TDqDataSerializer::SerializeParamValue(const TType* type, const NUdf::TUnboxedValuePod& value) {
    return SerializeValuePickleV1(type, value);
}

ui64 TDqDataSerializer::CalcSerializedSize(NUdf::TUnboxedValue& value, const NKikimr::NMiniKQL::TType* itemType) {
    auto data = SerializeValuePickleV1(itemType, value);
    // YQL-9648
    DeserializeValuePickleV1(itemType, data, value, HolderFactory);
    return data.GetRaw().size();
}

namespace {

std::optional<ui64> EstimateIntegralDataSize(const TDataType* dataType) {
    switch (*dataType->GetDataSlot()) {
        case NUdf::EDataSlot::Bool:
        case NUdf::EDataSlot::Int8:
        case NUdf::EDataSlot::Uint8:
            return 1;
        case NUdf::EDataSlot::Int16:
        case NUdf::EDataSlot::Uint16:
            return 2;
        case NUdf::EDataSlot::Int32:
        case NUdf::EDataSlot::Uint32:
        case NUdf::EDataSlot::Float:
        case NUdf::EDataSlot::Date:
        case NUdf::EDataSlot::TzDate:
        case NUdf::EDataSlot::Datetime:
        case NUdf::EDataSlot::TzDatetime:
            return 4;
        case NUdf::EDataSlot::Int64:
        case NUdf::EDataSlot::Uint64:
        case NUdf::EDataSlot::Double:
        case NUdf::EDataSlot::Timestamp:
        case NUdf::EDataSlot::TzTimestamp:
        case NUdf::EDataSlot::Interval:
            return 8;
        case NUdf::EDataSlot::Uuid:
        case NUdf::EDataSlot::Decimal:
            return 16;
        case NUdf::EDataSlot::String:
        case NUdf::EDataSlot::Utf8:
        case NUdf::EDataSlot::DyNumber:
        case NUdf::EDataSlot::Json:
        case NUdf::EDataSlot::JsonDocument:
        case NUdf::EDataSlot::Yson:
            return std::nullopt;
    }
}

ui64 EstimateSizeImpl(const NUdf::TUnboxedValuePod& value, const NKikimr::NMiniKQL::TType* type, bool* fixed, TDqDataSerializer::TEstimateSizeSettings settings) {
    switch (type->GetKind()) {
        case TType::EKind::Void:
        case TType::EKind::Null:
        case TType::EKind::EmptyList:
        case TType::EKind::EmptyDict:
            return 0;

        case TType::EKind::Data: {
            auto dataType = static_cast<const TDataType*>(type);
            if (auto size = EstimateIntegralDataSize(dataType); size.has_value()) {
                return *size;
            }
            if (fixed) {
                *fixed = false;
            }
            switch (*dataType->GetDataSlot()) {
                case NUdf::EDataSlot::String:
                case NUdf::EDataSlot::Utf8:
                case NUdf::EDataSlot::DyNumber:
                case NUdf::EDataSlot::Json:
                case NUdf::EDataSlot::JsonDocument:
                case NUdf::EDataSlot::Yson:
                    return (settings.WithHeaders?2:0) + value.AsStringRef().Size();
                default:
                    YQL_ENSURE(false, "" << dataType->GetKindAsStr());
            }
        }

        case TType::EKind::Optional: {
            auto optionalType = static_cast<const TOptionalType*>(type);
            if (value) {
                if (optionalType->GetItemType()->GetKind() == TType::EKind::Data) {
                    auto dataType = static_cast<const TDataType*>(optionalType->GetItemType());
                    if (auto size = EstimateIntegralDataSize(dataType); size.has_value()) {
                        return *size;
                    }
                }
                return EstimateSizeImpl(value.GetOptionalValue(), optionalType->GetItemType(), fixed, settings);
            }
            return 0;
        }

        case TType::EKind::List: {
            auto listType = static_cast<const TListType*>(type);
            auto itemType = listType->GetItemType();
            ui64 size = (settings.WithHeaders?2:0);
            if (value.HasFastListLength() && value.GetListLength() > 0 && value.GetElements()) {
                auto len = value.GetListLength();
                auto p = value.GetElements();
                do {
                    size += EstimateSizeImpl(*p++, itemType, fixed, settings);
                }
                while (--len);
            } else {
                const auto iter = value.GetListIterator();
                for (NUdf::TUnboxedValue item; iter.Next(item);) {
                    size += EstimateSizeImpl(item, itemType, fixed, settings);
                }
            }
            return size;
        }

        case TType::EKind::Struct: {
            auto structType = static_cast<const TStructType*>(type);
            ui64 size = (settings.WithHeaders?2:0);
            for (ui32 index = 0; index < structType->GetMembersCount(); ++index) {
                auto memberType = structType->GetMemberType(index);

                if (memberType->GetKind() == TType::EKind::Data) {
                    auto dataType = static_cast<const TDataType*>(memberType);
                    if (auto s = EstimateIntegralDataSize(dataType); s.has_value()) {
                        size += *s;
                        continue;
                    }
                }

                size += EstimateSizeImpl(value.GetElement(index), memberType, fixed, settings);
            }

            return size;
        }

        case TType::EKind::Tuple: {
            auto tupleType = static_cast<const TTupleType*>(type);
            ui64 size = (settings.WithHeaders?2:0);
            for (ui32 index = 0; index < tupleType->GetElementsCount(); ++index) {
                auto elementType = tupleType->GetElementType(index);

                if (elementType->GetKind() == TType::EKind::Data) {
                    auto dataType = static_cast<const TDataType*>(elementType);
                    if (auto s = EstimateIntegralDataSize(dataType); s.has_value()) {
                        size += *s;
                        continue;
                    }
                }

                size += EstimateSizeImpl(value.GetElement(index), elementType, fixed, settings);
            }
            return size;
        }

        case TType::EKind::Dict: {
            auto dictType = static_cast<const TDictType*>(type);
            auto keyType = dictType->GetKeyType();
            auto payloadType = dictType->GetPayloadType();

            ui64 size = (settings.WithHeaders?2:0);
            const auto iter = value.GetDictIterator();
            for (NUdf::TUnboxedValue key, payload; iter.NextPair(key, payload);) {
                size += EstimateSizeImpl(key, keyType, fixed, settings);
                size += EstimateSizeImpl(payload, payloadType, fixed, settings);
            }
            return size;
        }

        case TType::EKind::Variant: {
            auto variantType = static_cast<const TVariantType*>(type);
            ui32 variantIndex = value.GetVariantIndex();
            TType* innerType = variantType->GetUnderlyingType();
            if (innerType->IsStruct()) {
                innerType = static_cast<TStructType*>(innerType)->GetMemberType(variantIndex);
            } else {
                MKQL_ENSURE(innerType->IsTuple(), "Unexpected underlying variant type: " << innerType->GetKindAsStr());
                innerType = static_cast<TTupleType*>(innerType)->GetElementType(variantIndex);
            }
            return (settings.WithHeaders?2:0) + EstimateSizeImpl(value.GetVariantItem(), innerType, fixed, settings);
        }

        case TType::EKind::Pg: {
            if (value) {
                auto pgType = static_cast<const TPgType*>(type);
                return NKikimr::NMiniKQL::PgValueSize(pgType, value);
            }
            return 0;
        }
        case TType::EKind::Type:
        case TType::EKind::Stream:
        case TType::EKind::Callable:
        case TType::EKind::Any:
        case TType::EKind::Resource:
        case TType::EKind::Flow:
        case TType::EKind::ReservedKind:
        case TType::EKind::Tagged:
        case TType::EKind::Block: {
            if (settings.DiscardUnsupportedTypes) {
                return 0;
            }
            THROW yexception() << "Unsupported type: " << type->GetKindAsStr();
        }
    }
}

} // namespace

ui64 TDqDataSerializer::EstimateSize(const NUdf::TUnboxedValue& value, const NKikimr::NMiniKQL::TType* type, bool* fixed, TDqDataSerializer::TEstimateSizeSettings settings)
{
    if (fixed) {
        *fixed = true;
    }
    return EstimateSizeImpl(value, type, fixed, settings);
}

} // namespace NYql::NDq
