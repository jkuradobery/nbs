#pragma once

#include "test_utils.h"

#include <ydb/core/testlib/test_pq_client.h>

#include <ydb/library/aclib/aclib.h>


#include <ydb/public/sdk/cpp/client/ydb_driver/driver.h>
#include <ydb/public/sdk/cpp/client/ydb_table/table.h>
#include <ydb/public/sdk/cpp/client/ydb_persqueue_core/persqueue.h>
#include <ydb/public/sdk/cpp/client/ydb_persqueue_core/ut/ut_utils/test_server.h>

#include <util/system/env.h>

namespace NKikimr::NPersQueueTests {



#define SET_LOCALS                                              \
    auto& pqClient = server.Server->AnnoyingClient;             \
    Y_UNUSED(pqClient);                                         \
    auto* runtime = server.Server->CleverServer->GetRuntime();  \
    Y_UNUSED(runtime);                                          \


    using namespace Tests;
    using namespace NKikimrClient;
    using namespace Ydb::PersQueue;
    using namespace Ydb::PersQueue::V1;
    using namespace NThreading;
    using namespace NNetClassifier;

    class TPersQueueV1TestServerBase {
    public:
        virtual void AlterSettings(NKikimr::Tests::TServerSettings& settings) {
            Y_UNUSED(settings);
        }
        void InitializePQ() {
            Y_VERIFY(Server == nullptr);
            PortManager = new TPortManager();
            Server = MakeHolder<NPersQueue::TTestServer>(false, PortManager);
            Server->ServerSettings.PQConfig.SetTopicsAreFirstClassCitizen(TenantModeEnabled());
            Server->ServerSettings.PQConfig.MutablePQDiscoveryConfig()->SetLBFrontEnabled(true);
            AlterSettings(Server->ServerSettings);
            Server->StartServer(false);
            if (TenantModeEnabled()) {
                Server->AnnoyingClient->SetNoConfigMode();
                Server->ServerSettings.PQConfig.SetSourceIdTablePath("some unused path");
            }
            Cerr << "Init PQ - start server on port " << Server->GrpcPort << Endl;
            Server->GrpcServerOptions.SetMaxMessageSize(130_MB);
            EnablePQLogs({NKikimrServices::PQ_READ_PROXY, NKikimrServices::PQ_WRITE_PROXY, NKikimrServices::FLAT_TX_SCHEMESHARD});
            EnablePQLogs({NKikimrServices::PERSQUEUE}, NLog::EPriority::PRI_INFO);
            EnablePQLogs({NKikimrServices::KQP_PROXY}, NLog::EPriority::PRI_EMERG);

            Server->AnnoyingClient->FullInit();
            Server->AnnoyingClient->CreateConsumer("user");
            if (TenantModeEnabled()) {
                Cerr << "Will create fst-class topics\n";
                Server->AnnoyingClient->CreateTopicNoLegacy("/Root/acc/topic1", 1);
                Server->AnnoyingClient->CreateTopicNoLegacy("/Root/PQ/acc/topic1", 1);
            } else {
                Cerr << "Will create legacy-style topics\n";
                Server->AnnoyingClient->CreateTopicNoLegacy("rt3.dc1--acc--topic2dc", 1);
                Server->AnnoyingClient->CreateTopicNoLegacy("rt3.dc2--acc--topic2dc", 1, true, false);
                Server->AnnoyingClient->CreateTopicNoLegacy("rt3.dc1--topic1", 1);
                Server->AnnoyingClient->CreateTopicNoLegacy("rt3.dc1--acc--topic1", 1);
                Server->WaitInit("topic1");
                Sleep(TDuration::Seconds(10));
            }
            EnablePQLogs({ NKikimrServices::KQP_PROXY }, NLog::EPriority::PRI_EMERG);

            InsecureChannel = grpc::CreateChannel("localhost:" + ToString(Server->GrpcPort), grpc::InsecureChannelCredentials());
            ServiceStub = Ydb::PersQueue::V1::PersQueueService::NewStub(InsecureChannel);
            InitializeWritePQService(TenantModeEnabled() ? "Root/acc/topic1" : "topic1");

            NYdb::TDriverConfig driverCfg;
            driverCfg.SetEndpoint(TStringBuilder() << "localhost:" << Server->GrpcPort).SetLog(CreateLogBackend("cerr", ELogPriority::TLOG_DEBUG)).SetDatabase("/Root");
            YdbDriver.reset(new NYdb::TDriver(driverCfg));
            PersQueueClient = MakeHolder<NYdb::NPersQueue::TPersQueueClient>(*YdbDriver);
        }

        void EnablePQLogs(const TVector<NKikimrServices::EServiceKikimr> services,
                        NActors::NLog::EPriority prio = NActors::NLog::PRI_DEBUG)
        {
            for (auto s : services) {
                Server->CleverServer->GetRuntime()->SetLogPriority(s, prio);
            }
        }

        void InitializeWritePQService(const TString &topicToWrite) {
            while (true) {
                Sleep(TDuration::MilliSeconds(100));

                Ydb::PersQueue::V1::StreamingWriteClientMessage req;
                Ydb::PersQueue::V1::StreamingWriteServerMessage resp;
                grpc::ClientContext context;

                auto stream = ServiceStub->StreamingWrite(&context);
                UNIT_ASSERT(stream);

                req.mutable_init_request()->set_topic(topicToWrite);
                req.mutable_init_request()->set_message_group_id("12345678");

                if (!stream->Write(req)) {
                    UNIT_ASSERT_C(stream->Read(&resp), "Context error: " << context.debug_error_string());
                    UNIT_ASSERT_C(resp.status() == Ydb::StatusIds::UNAVAILABLE,
                                  "Response: " << resp << ", Context error: " << context.debug_error_string());
                    continue;
                }

                AssertSuccessfullStreamingOperation(stream->Read(&resp), stream);
                if (resp.status() == Ydb::StatusIds::UNAVAILABLE) {
                    continue;
                }

                if (stream->WritesDone()) {
                    auto status = stream->Finish();
                    Cerr << "Finish: " << (int) status.error_code() << " " << status.error_message() << "\n";
                }

                break;
            }
        }

    public:
        static bool TenantModeEnabled() {
            return !GetEnv("PERSQUEUE_NEW_SCHEMECACHE").empty();
        }

        void ModifyTopicACL(const TString& topic, const NACLib::TDiffACL& acl) {
            if (TenantModeEnabled()) {
                TFsPath path(topic);
                Server->AnnoyingClient->ModifyACL(path.Dirname(), path.Basename(), acl.SerializeAsString());
            } else {
                Server->AnnoyingClient->ModifyACL("/Root/PQ", topic, acl.SerializeAsString());
            }
            WaitACLModification();

        }

        TString GetRoot() const {
            return !TenantModeEnabled() ? "/Root/PQ" : "";
        }

        TString GetTopic() {
            return TenantModeEnabled() ? "/Root/acc/topic1" : "rt3.dc1--topic1";
        }

        TString GetTopicPath() {
            return TenantModeEnabled() ? "/Root/acc/topic1" : "topic1";
        }

        TString GetTopicPathMultipleDC() const {
            return "acc/topic2dc";
        }

    public:
        THolder<NPersQueue::TTestServer> Server;
        TSimpleSharedPtr<TPortManager> PortManager;
        std::shared_ptr<grpc::Channel> InsecureChannel;
        std::unique_ptr<Ydb::PersQueue::V1::PersQueueService::Stub> ServiceStub;

        std::shared_ptr<NYdb::TDriver> YdbDriver;
        THolder<NYdb::NPersQueue::TPersQueueClient> PersQueueClient;
    };

    class TPersQueueV1TestServer : public TPersQueueV1TestServerBase {
    public:
        TPersQueueV1TestServer(bool checkAcl = false)
            : CheckACL(checkAcl)
        {
            InitAll();
        }

        void InitAll() {
            InitializePQ();
        }

        void AlterSettings(NKikimr::Tests::TServerSettings& settings) override {
            if (CheckACL)
                settings.PQConfig.SetCheckACL(true);
        }
    private:
        bool CheckACL;
    };

    class TPersQueueV1TestServerWithRateLimiter : public TPersQueueV1TestServerBase {
    private:
        NKikimrPQ::TPQConfig::TQuotingConfig::ELimitedEntity LimitedEntity;
    public:
        TPersQueueV1TestServerWithRateLimiter()
            : TPersQueueV1TestServerBase()
        {}

        void AlterSettings(NKikimr::Tests::TServerSettings& settings) override {
            settings.PQConfig.MutableQuotingConfig()->SetEnableQuoting(true);
            settings.PQConfig.MutableQuotingConfig()->SetTopicWriteQuotaEntityToLimit(LimitedEntity);
        }

        void InitAll(NKikimrPQ::TPQConfig::TQuotingConfig::ELimitedEntity limitedEntity) {
            LimitedEntity = limitedEntity;
            InitializePQ();
            InitQuotingPaths();
        }

        void InitQuotingPaths() {
            Server->AnnoyingClient->MkDir("/Root", "PersQueue");
            Server->AnnoyingClient->MkDir("/Root/PersQueue", "System");
            Server->AnnoyingClient->MkDir("/Root/PersQueue/System", "Quoters");
        }

        void CreateTopicWithQuota(const TString& path, bool createKesus = true, double writeQuota = 1000.0) {
            TVector<TString> pathComponents = SplitPath(path);
            const TString account = pathComponents[0];
            const TString name = NPersQueue::BuildFullTopicName(path, "dc1");

            if (TenantModeEnabled()) {
                Server->AnnoyingClient->CreateTopicNoLegacy("/Root/PQ/" + path, 1);
            } else {
                Cerr << "Creating topic \"" << name << "\"" << Endl;
                Server->AnnoyingClient->CreateTopicNoLegacy(name, 1);
            }

            const TString rootPath = "/Root/PersQueue/System/Quoters";
            const TString kesusPath = TStringBuilder() << rootPath << "/" << account;

            if (createKesus) {
                Cerr << "Creating kesus \"" << account << "\"" << Endl;
                const NMsgBusProxy::EResponseStatus createKesusResult = Server->AnnoyingClient->CreateKesus(rootPath, account);
                UNIT_ASSERT_C(createKesusResult == NMsgBusProxy::MSTATUS_OK, createKesusResult);

                const auto statusCode = Server->AnnoyingClient->AddQuoterResource(
                        Server->CleverServer->GetRuntime(), kesusPath, "write-quota", writeQuota
                );
                UNIT_ASSERT_EQUAL_C(statusCode, Ydb::StatusIds::SUCCESS, "Status: " << Ydb::StatusIds::StatusCode_Name(statusCode));
            }

            if (pathComponents.size() > 1) { // account (first component) + path of topic
                TStringBuilder prefixPath; // path without account
                prefixPath << "write-quota";
                for (auto currentComponent = pathComponents.begin() + 1; currentComponent != pathComponents.end(); ++currentComponent) {
                    prefixPath << "/" << *currentComponent;
                    Cerr << "Adding quoter resource: \"" << prefixPath << "\"" << Endl;
                    const auto statusCode = Server->AnnoyingClient->AddQuoterResource(
                            Server->CleverServer->GetRuntime(), kesusPath, prefixPath
                    );
                    UNIT_ASSERT_C(statusCode == Ydb::StatusIds::SUCCESS || statusCode == Ydb::StatusIds::ALREADY_EXISTS, "Status: " << Ydb::StatusIds::StatusCode_Name(statusCode));
                }
            }
        }
/*
        THolder<IProducer> StartProducer(const TString& topicPath, bool compress = false) {
            TString fullPath = TenantModeEnabled() ? "/Root/PQ/" + topicPath : topicPath;
            TProducerSettings producerSettings;
            producerSettings.Server = TServerSetting("localhost", Server->GrpcPort);
            producerSettings.Topic = fullPath;
            producerSettings.SourceId = "TRateLimiterTestSetupSourceId";
            producerSettings.Codec = compress ? "gzip" : "raw";
            THolder<IProducer> producer = PQLib->CreateProducer(producerSettings);
            auto startResult = producer->Start();
            UNIT_ASSERT_EQUAL_C(Ydb::StatusIds::SUCCESS, startResult.GetValueSync().Response.status(), "Response: " << startResult.GetValueSync().Response);
            return producer;
        }
*/
    };
}

