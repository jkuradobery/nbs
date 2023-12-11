#include "actors.h"
#include <contrib/ydb/core/kafka_proxy/kafka_events.h>

#include <contrib/ydb/library/actors/core/actor_bootstrapped.h>

namespace NKafka {

struct TopicEntities {
    std::shared_ptr<TSet<TString>> Consumers = std::make_shared<TSet<TString>>();
    std::shared_ptr<TSet<ui32>> Partitions = std::make_shared<TSet<ui32>>();
};

class TKafkaOffsetFetchActor: public NActors::TActorBootstrapped<TKafkaOffsetFetchActor> {
public:
    TKafkaOffsetFetchActor(const TContext::TPtr context, const ui64 correlationId, const TMessagePtr<TOffsetFetchRequestData>& message)
        : Context(context)
        , CorrelationId(correlationId)
        , Message(message) {
    }

    void Bootstrap(const NActors::TActorContext& ctx);
    void StateWork(TAutoPtr<IEventHandle>& ev);
    void Handle(TEvKafka::TEvCommitedOffsetsResponse::TPtr& ev, const TActorContext& ctx);
    void ExtractPartitions(const TString& group, const NKafka::TOffsetFetchRequestData::TOffsetFetchRequestGroup::TOffsetFetchRequestTopics& topic);
    TOffsetFetchResponseData::TPtr GetOffsetFetchResponse();

private:
    const TContext::TPtr Context;
    const ui64 CorrelationId;
    const TMessagePtr<TOffsetFetchRequestData> Message;
    std::unordered_map<TString, TopicEntities> TopicToEntities_;
    std::unordered_map<TString, std::shared_ptr<std::unordered_map<ui32, std::unordered_map<TString, ui32>>>> TopicToOffsets_;
    ui32 InflyTopics_ = 0;

};

} // NKafka
