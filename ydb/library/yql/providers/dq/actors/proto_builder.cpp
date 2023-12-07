#include "proto_builder.h"

#include <ydb/library/yql/providers/common/codec/yql_codec.h>
#include <ydb/library/yql/dq/proto/dq_transport.pb.h>
#include <ydb/library/yql/dq/runtime/dq_transport.h>
#include <ydb/library/yql/minikql/mkql_node_cast.h>
#include <ydb/library/yql/minikql/mkql_node_serialization.h>
#include <ydb/library/yql/utils/log/log.h>

#include <ydb/library/mkql_proto/mkql_proto.h>

#include <library/cpp/yson/node/node_io.h>
#include <library/cpp/yson/writer.h>

namespace NYql::NDqs {

using namespace NKikimr::NMiniKQL;

namespace {

TVector<ui32> BuildColumnOrder(const TVector<TString>& columns, NKikimr::NMiniKQL::TType* resultType) {
    MKQL_ENSURE(resultType, "Incorrect result type");
    if (resultType->GetKind() != TType::EKind::Struct || columns.empty()) {
        return {};
    }

    TVector<ui32> columnOrder;
    THashMap<TString, ui32> column2id;
    auto structType = AS_TYPE(TStructType, resultType);
    for (ui32 i = 0; i < structType->GetMembersCount(); ++i) {
        const auto columnName = TString(structType->GetMemberName(i));
        column2id[columnName] = i;
    }
    columnOrder.resize(columns.size());

    int id = 0;
    for (const auto& columnName : columns) {
        columnOrder[id++] = column2id[columnName];
    }
    return columnOrder;
}

NDqProto::EDataTransportVersion GetTransportVersion(const NYql::NDqProto::TData& data) {
    switch (data.GetTransportVersion()) {
        case 10000:
            return NDqProto::EDataTransportVersion::DATA_TRANSPORT_YSON_1_0;
        case 20000:
            return NDqProto::EDataTransportVersion::DATA_TRANSPORT_UV_PICKLE_1_0;
        case 30000:
            return NDqProto::EDataTransportVersion::DATA_TRANSPORT_ARROW_1_0;
        default:
            break;
    }
    return NDqProto::EDataTransportVersion::DATA_TRANSPORT_VERSION_UNSPECIFIED;
}

} // unnamed

TProtoBuilder::TProtoBuilder(const TString& type, const TVector<TString>& columns)
    : Alloc(__LOCATION__)
    , TypeEnv(Alloc)
    , ResultType(static_cast<TType*>(DeserializeNode(type, TypeEnv)))
    , ColumnOrder(BuildColumnOrder(columns, ResultType))
{
    Alloc.Release();
}

TProtoBuilder::~TProtoBuilder() {
    Alloc.Acquire();
}

bool TProtoBuilder::CanBuildResultSet() const {
    return ResultType->GetKind() == TType::EKind::Struct;
}

TString TProtoBuilder::BuildYson(const TVector<NYql::NDqProto::TData>& rows, ui64 maxBytesLimit) {
    ui64 size = 0;
    TStringStream out;
    NYson::TYsonWriter writer((IOutputStream*)&out);
    writer.OnBeginList();

    auto full = WriteData(rows, [&](const NYql::NUdf::TUnboxedValuePod& value) {
        auto rowYson = NCommon::WriteYsonValue(value, ResultType, ColumnOrder.empty() ? nullptr : &ColumnOrder);
        writer.OnListItem();
        writer.OnRaw(rowYson);
        size += rowYson.size();
        return size <= maxBytesLimit;
    });

    if (!full) {
        ythrow yexception() << "Too big yson result size: " << size << " > " << maxBytesLimit;
    }

    writer.OnEndList();
    return out.Str();
}

bool TProtoBuilder::WriteYsonData(const NYql::NDqProto::TData& data, const std::function<bool(const TString& rawYson)>& func) {
    return WriteData(data, [&](const NYql::NUdf::TUnboxedValuePod& value) {
        auto rowYson = NCommon::WriteYsonValue(value, ResultType, ColumnOrder.empty() ? nullptr : &ColumnOrder);
        return func(rowYson);
    });
}

bool TProtoBuilder::WriteData(const NDqProto::TData& data, const std::function<bool(const NYql::NUdf::TUnboxedValuePod& value)>& func) {
    TGuard<TScopedAlloc> allocGuard(Alloc);

    TMemoryUsageInfo memInfo("ProtoBuilder");
    THolderFactory holderFactory(Alloc.Ref(), memInfo);
    NDqProto::EDataTransportVersion transportVersion = GetTransportVersion(data);
    NDq::TDqDataSerializer dataSerializer(TypeEnv, holderFactory, transportVersion);

    TUnboxedValueVector buffer;
    dataSerializer.Deserialize(data, ResultType, buffer);

    for (const auto& item : buffer) {
        if (!func(item)) {
            return false;
        }
    }
    return true;
}

bool TProtoBuilder::WriteData(const TVector<NDqProto::TData>& rows, const std::function<bool(const NYql::NUdf::TUnboxedValuePod& value)>& func) {
    TGuard<TScopedAlloc> allocGuard(Alloc);

    TMemoryUsageInfo memInfo("ProtoBuilder");
    THolderFactory holderFactory(Alloc.Ref(), memInfo);
    const auto transportVersion = rows.empty() ? NDqProto::EDataTransportVersion::DATA_TRANSPORT_VERSION_UNSPECIFIED : GetTransportVersion(rows.front());
    NDq::TDqDataSerializer dataSerializer(TypeEnv, holderFactory, transportVersion);

    for (const auto& part : rows) {
        TUnboxedValueVector buffer;
        dataSerializer.Deserialize(part, ResultType, buffer);
        for (const auto& item : buffer) {
            if (!func(item)) {
                return false;
            }
        }
    }
    return true;
}

Ydb::ResultSet TProtoBuilder::BuildResultSet(const TVector<NYql::NDqProto::TData>& data) {
    Ydb::ResultSet resultSet;
    auto structType = AS_TYPE(TStructType, ResultType);
    MKQL_ENSURE(structType, "Result is not a struct");
    for (ui32 i = 0; i < structType->GetMembersCount(); ++i) {
        auto& column = *resultSet.add_columns();
        const ui32 memberIndex = ColumnOrder.empty() ? i : ColumnOrder[i];
        column.set_name(TString(structType->GetMemberName(memberIndex)));
        ExportTypeToProto(structType->GetMemberType(memberIndex), *column.mutable_type());
    }

    WriteData(data, [&](const NYql::NUdf::TUnboxedValuePod& value) {
        ExportValueToProto(ResultType, value, *resultSet.add_rows(), &ColumnOrder);
        return true;
    });

    return resultSet;
}

TString TProtoBuilder::GetSerializedType() const {
    auto result = SerializeNode(ResultType, TypeEnv);
    return result;
}

TString TProtoBuilder::AllocDebugInfo() {
    TGuard<TScopedAlloc> allocGuard(Alloc);
    return TStringBuilder{} << "Used:           " << Alloc.GetUsed() << '\n'
                            << "Peak used:      " << Alloc.GetPeakUsed() << '\n'
                            << "Allocated:      " << Alloc.GetAllocated() << '\n'
                            << "Peak allocated: " << Alloc.GetPeakAllocated() << '\n'
                            << "Limit:          " << Alloc.GetLimit();
}

} // NYql::NDqs
