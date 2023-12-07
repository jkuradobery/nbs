#include "dq_input_channel.h"
#include "dq_input_impl.h"

namespace NYql::NDq {

class TDqInputChannel : public TDqInputImpl<TDqInputChannel, IDqInputChannel> {
    using TBaseImpl = TDqInputImpl<TDqInputChannel, IDqInputChannel>;
    friend TBaseImpl;
private:
    std::deque<NDqProto::TData> DataForDeserialize;

    void PushImpl(NDqProto::TData&& data) {
        const i64 space = data.GetRaw().size();

        NKikimr::NMiniKQL::TUnboxedValueVector buffer;
        buffer.reserve(data.GetRows());

        if (Y_UNLIKELY(ProfileStats)) {
            auto startTime = TInstant::Now();
            DataSerializer.Deserialize(data, InputType, buffer);
            ProfileStats->DeserializationTime += (TInstant::Now() - startTime);
        } else {
            DataSerializer.Deserialize(data, InputType, buffer);
        }

        AddBatch(std::move(buffer), space);
    }

public:
    TDqInputChannel(ui64 channelId, NKikimr::NMiniKQL::TType* inputType, ui64 maxBufferBytes, bool collectProfileStats,
        const NKikimr::NMiniKQL::TTypeEnvironment& typeEnv, const NKikimr::NMiniKQL::THolderFactory& holderFactory,
        NDqProto::EDataTransportVersion transportVersion)
        : TBaseImpl(inputType, maxBufferBytes)
        , ChannelId(channelId)
        , BasicStats(ChannelId)
        , ProfileStats(collectProfileStats ? &BasicStats : nullptr)
        , DataSerializer(typeEnv, holderFactory, transportVersion)
    {}

    ui64 GetChannelId() const override {
        return ChannelId;
    }

    [[nodiscard]]
    bool Pop(NKikimr::NMiniKQL::TUnboxedValueVector& batch) override {
        if (Batches.empty()) {
            if (DataForDeserialize.size()) {
                PushImpl(std::move(DataForDeserialize.front()));
                DataForDeserialize.pop_front();
                return TBaseImpl::Pop(batch);
            } else {
                return false;
            }
        } else {
            return TBaseImpl::Pop(batch);
        }
    }

    void Push(NDqProto::TData&& data) override {
        YQL_ENSURE(!Finished, "input channel " << ChannelId << " already finished");
        if (Y_UNLIKELY(data.GetRows() == 0)) {
            return;
        }
        DataForDeserialize.emplace_back(std::move(data));
    }

    const TDqInputChannelStats* GetStats() const override {
        return &BasicStats;
    }

private:
    const ui64 ChannelId;
    TDqInputChannelStats BasicStats;
    TDqInputChannelStats* ProfileStats = nullptr;
    TDqDataSerializer DataSerializer;
};

IDqInputChannel::TPtr CreateDqInputChannel(ui64 channelId, NKikimr::NMiniKQL::TType* inputType, ui64 maxBufferBytes,
    bool collectProfileStats, const NKikimr::NMiniKQL::TTypeEnvironment& typeEnv,
    const NKikimr::NMiniKQL::THolderFactory& holderFactory, NDqProto::EDataTransportVersion transportVersion)
{
    return new TDqInputChannel(channelId, inputType, maxBufferBytes, collectProfileStats, typeEnv, holderFactory,
        transportVersion);
}

} // namespace NYql::NDq
