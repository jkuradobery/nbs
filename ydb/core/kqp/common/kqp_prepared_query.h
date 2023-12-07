#pragma once

#include <ydb/core/protos/kqp.pb.h>

#include <util/generic/vector.h>

#include <memory>
#include <vector>

namespace NKikimr {
namespace NMiniKQL {
class IFunctionRegistry;
class TScopedAlloc;
class TTypeEnvironment;
class TType;
}
}

namespace NKqpProto {
class TKqpPhyTx;
}

namespace NKikimr::NKqp {

class TPreparedQueryAllocHolder;

struct TPhyTxResultMetadata {
    bool IsStream = false;
    NKikimr::NMiniKQL::TType* MkqlItemType;
    TVector<ui32> ColumnOrder;
};

class TKqpPhyTxHolder {
    std::shared_ptr<const NKikimrKqp::TPreparedQuery> PreparedQuery;
    const NKqpProto::TKqpPhyTx* Proto;
    bool PureTx = false;
    TVector<TPhyTxResultMetadata> TxResultsMeta;
    std::shared_ptr<TPreparedQueryAllocHolder> Alloc;

public:
    using TConstPtr = std::shared_ptr<const TKqpPhyTxHolder>;

    const TVector<TPhyTxResultMetadata>& GetTxResultsMeta() const { return TxResultsMeta; }

    const NKqpProto::TKqpPhyStage& GetStages(size_t index) const {
        return Proto->GetStages(index);
    }

    size_t StagesSize() const {
        return Proto->StagesSize();
    }

    NKqpProto::TKqpPhyTx_EType GetType() const {
        return Proto->GetType();
    }

    const TProtoStringType& GetPlan() const {
        return Proto->GetPlan();
    }

    size_t ResultsSize() const {
        return Proto->ResultsSize();
    }

    const NKqpProto::TKqpPhyResult& GetResults(size_t index) const {
        return Proto->GetResults(index);
    }

    const google::protobuf::RepeatedPtrField< ::NKqpProto::TKqpPhyStage>& GetStages() const {
        return Proto->GetStages();
    }

    bool GetHasEffects() const {
        return Proto->GetHasEffects();
    }

    const ::google::protobuf::RepeatedPtrField< ::NKqpProto::TKqpPhyParamBinding> & GetParamBindings() const {
        return Proto->GetParamBindings();
    }

    const google::protobuf::RepeatedPtrField< ::NKqpProto::TKqpPhyTable>& GetTables() const {
        return Proto->GetTables();
    }

    TProtoStringType DebugString() const {
        return Proto->ShortDebugString();
    }

    TKqpPhyTxHolder(const std::shared_ptr<const NKikimrKqp::TPreparedQuery>& pq, const NKqpProto::TKqpPhyTx* proto,
        const std::shared_ptr<TPreparedQueryAllocHolder>& alloc);

    bool IsPureTx() const;
};

class TPreparedQueryHolder {
    std::shared_ptr<const NKikimrKqp::TPreparedQuery> Proto;
    std::shared_ptr<TPreparedQueryAllocHolder> Alloc;
    TVector<TString> QueryTables;
    std::vector<TKqpPhyTxHolder::TConstPtr> Transactions;

public:

    TPreparedQueryHolder(NKikimrKqp::TPreparedQuery* proto, const NKikimr::NMiniKQL::IFunctionRegistry* functionRegistry);
    ~TPreparedQueryHolder();

    using TConstPtr = std::shared_ptr<const TPreparedQueryHolder>;

    const std::vector<TKqpPhyTxHolder::TConstPtr>& GetTransactions() const {
        return Transactions;
    }

    const ::google::protobuf::RepeatedPtrField< ::NKikimrKqp::TParameterDescription>& GetParameters() const {
        return Proto->GetParameters();
    }

    const TKqpPhyTxHolder::TConstPtr& GetPhyTx(ui32 idx) const;
    TKqpPhyTxHolder::TConstPtr GetPhyTxOrEmpty(ui32 idx) const;

    TString GetText() const;

    ui32 GetVersion() const {
        return Proto->GetVersion();
    }

    size_t ResultsSize() const {
        return Proto->ResultsSize();
    }

    const NKikimrKqp::TPreparedResult& GetResults(size_t index) const {
        return Proto->GetResults(index);
    }

    ui64 ByteSize() const {
        return Proto->ByteSize();
    }

    const TVector<TString>& GetQueryTables() const {
        return QueryTables;
    }

    const NKqpProto::TKqpPhyQuery& GetPhysicalQuery() const {
        return Proto->GetPhysicalQuery();
    }

    std::optional<bool> GetEnableLlvm() const {
        if (Proto->HasEnableLlvm()) {
            return Proto->GetEnableLlvm();
         } else {
            return std::nullopt;
         }
    }
};


} // namespace NKikimr::NKqp
