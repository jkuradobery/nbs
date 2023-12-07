#include <ydb/services/ydb/ydb_common_ut.h>
#include <ydb/public/sdk/cpp/client/ydb_driver/driver.h>

#include <ydb/public/lib/yq/yq.h>
#include <ydb/public/lib/yq/helpers.h>
#include <ydb/core/yq/libs/db_schema/db_schema.h>
#include <ydb/core/yq/libs/mock/yql_mock.h>
#include <ydb/core/yq/libs/private_client/private_client.h>

#include <ydb/core/yq/libs/control_plane_storage/message_builders.h>
#include <ydb/core/yq/libs/actors/database_resolver.h>

#include <ydb/library/yql/public/issue/yql_issue_message.h>

#include <library/cpp/protobuf/util/pb_io.h>
#include <library/cpp/retry/retry.h>
#include <util/system/mutex.h>
#include "ut_utils.h"
#include <google/protobuf/util/time_util.h>

#include <ydb/public/lib/yq/scope.h>
#include <util/system/hostname.h>

#include <util/string/split.h>

using namespace NYdb;
using namespace YandexQuery;;
using namespace NYdb::NYq;

namespace {
    const ui32 Retries = 10;

    void PrintProtoIssues(const NProtoBuf::RepeatedPtrField<::Ydb::Issue::IssueMessage>& protoIssues) {
        if (protoIssues.empty()) {
            Cerr << "No Issues" << Endl;
            return;
        }
        NYql::TIssues issues;
        NYql::IssuesFromMessage(protoIssues, issues);
        Cerr << ">>> Issues: " << issues.ToString() << Endl;
    }

    TString CreateNewHistoryAndWaitFinish(const TString& folderId,
        NYdb::NYq::TClient& client, const TString& yqlText,
        const YandexQuery::QueryMeta::ComputeStatus& expectedStatusResult)
    {
        //CreateQuery
        TString queryId;
        {
            auto request = ::NYq::TCreateQueryBuilder{}
                                .SetText(yqlText)
                                .Build();
            auto result = client.CreateQuery(
                request, CreateYqSettings<TCreateQuerySettings>(folderId))
                .ExtractValueSync();

            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            queryId = result.GetResult().query_id();
        }
        // GetQueryStatus
        const auto request = ::NYq::TGetQueryStatusBuilder{}
            .SetQueryId(queryId)
            .Build();
        const auto result = DoWithRetryOnRetCode([&]() {
            auto result = client.GetQueryStatus(
                request, CreateYqSettings<TGetQueryStatusSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            return result.GetResult().status() == expectedStatusResult;
        }, TRetryOptions(Retries));
        UNIT_ASSERT_C(result, "the execution of the query did not end within the time limit");

        return queryId;
    }

    void CheckGetResultData(
        NYdb::NYq::TClient& client,
        const TString& queryId,
        const TString& folderId,
        const ui64 rowsCount,
        const int colsSize,
        const int answer)
    {
        const auto request = ::NYq::TGetResultDataBuilder{}
            .SetQueryId(queryId)
            .Build();
        const auto result = client.GetResultData(
                request, CreateYqSettings<TGetResultDataSettings>(folderId))
                .ExtractValueSync();
        UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());

        const auto& resultSet = result.GetResult().result_set();
        UNIT_ASSERT_VALUES_EQUAL(resultSet.rows().size(), rowsCount);
        UNIT_ASSERT_VALUES_EQUAL(resultSet.columns().size(), colsSize);
        if (!resultSet.rows().empty()) {
            const auto& item = resultSet.rows(0).items(0);
            TString str = item.DebugString();
            TVector<TString> arr;
            StringSplitter(str).Split(' ').SkipEmpty().AddTo(&arr);
            Y_VERIFY(arr.size() == 2, "Incorrect numeric result");
            UNIT_ASSERT_VALUES_EQUAL(std::stoi(arr.back()), answer);
        }
    }
}

Y_UNIT_TEST_SUITE(Yq_1) {
    Y_UNIT_TEST(Basic) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc        = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver      = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);
        const auto folderId = "some_folder_id";
        const auto queryId = CreateNewHistoryAndWaitFinish(folderId, client, "select 1", YandexQuery::QueryMeta::COMPLETED);
        CheckGetResultData(client, queryId, folderId, 1, 1, 1);

        {
            const auto request = ::NYq::TDescribeQueryBuilder()
                .SetQueryId("foo")
                .Build();
            const auto result = DoWithRetryOnRetCode([&]() {
                auto result = client.DescribeQuery(
                    request, CreateYqSettings<TDescribeQuerySettings>("WTF"))
                    .ExtractValueSync();
                return result.GetStatus() == EStatus::BAD_REQUEST;
            }, TRetryOptions(Retries));
            UNIT_ASSERT_C(result, "the execution of the query did not end within the time limit");
        }

        {
            auto request = ::NYq::TListQueriesBuilder{}.Build();
            auto result = DoWithRetryOnRetCode([&]() {
                auto result = client.ListQueries(
                    request, CreateYqSettings<TListQueriesSettings>("WTF"))
                    .ExtractValueSync();
                UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
                UNIT_ASSERT_VALUES_EQUAL(result.GetResult().query().size(), 0);
                return result.GetStatus() == EStatus::SUCCESS;
            }, TRetryOptions(Retries));
            UNIT_ASSERT_C(result, "the execution of the query did not end within the time limit");
        }

        {
            const auto request = ::NYq::TDescribeQueryBuilder()
                .SetQueryId(queryId)
                .Build();
            auto result = client.DescribeQuery(
                request, CreateYqSettings<TDescribeQuerySettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            const auto status = result.GetResult().query().meta().status();
            UNIT_ASSERT(status == YandexQuery::QueryMeta::COMPLETED);
        }

        {
            const auto request = ::NYq::TModifyQueryBuilder()
                .SetQueryId(queryId)
                .SetName("MODIFIED_NAME")
                .Build();
            const auto result = client.ModifyQuery(
                request, CreateYqSettings<TModifyQuerySettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
        }

        {
            const auto request = ::NYq::TDescribeQueryBuilder()
                .SetQueryId(queryId)
                .Build();
            const auto result = client.DescribeQuery(
                request, CreateYqSettings<TDescribeQuerySettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            const auto& res = result.GetResult();
            UNIT_ASSERT_VALUES_EQUAL(res.query().content().name(), "MODIFIED_NAME");
            UNIT_ASSERT(res.query().content().acl().visibility() == static_cast<int>(Acl_Visibility_SCOPE));
        }

        {
            const auto request = ::NYq::TDescribeQueryBuilder()
                .SetQueryId("")
                .Build();
            const auto result = client.DescribeQuery(
                request, CreateYqSettings<TDescribeQuerySettings>(""))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::BAD_REQUEST, result.GetIssues().ToString());
        }

        {
            const auto request = ::NYq::TGetResultDataBuilder()
                .SetQueryId("")
                .Build();
            const auto result = client.GetResultData(
                    request, CreateYqSettings<TGetResultDataSettings>(""))
                    .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::BAD_REQUEST, result.GetIssues().ToString());
        }
    }

    Y_UNIT_TEST(Basic_EmptyTable) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        UpsertToExistingTable(driver, location);
        NYdb::NYq::TClient client(driver);
        const TString folderId = "some_folder_id";
        {
            const auto request = ::NYq::TCreateConnectionBuilder()
                .SetName("testdbempty")
                .CreateYdb("Root", location, "")
                .Build();
            const auto result = client
                .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
        }

        const TString queryId = CreateNewHistoryAndWaitFinish(
            folderId, client,
            "select count(*) from testdbempty.`yq/empty_table`",
            YandexQuery::QueryMeta::COMPLETED);
        CheckGetResultData(client, queryId, folderId, 1, 1, 0);
    }

    Y_UNIT_TEST(Basic_EmptyList) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);
        const TString folderId = "some_folder_id";
        auto expectedStatus = YandexQuery::QueryMeta::COMPLETED;
        CreateNewHistoryAndWaitFinish(folderId, client, "select []", expectedStatus);
    }

    Y_UNIT_TEST(Basic_EmptyDict) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);
        const TString folderId = "some_folder_id";
        auto expectedStatus = YandexQuery::QueryMeta::COMPLETED;
        CreateNewHistoryAndWaitFinish(folderId, client, "select {}", expectedStatus);
    }

    Y_UNIT_TEST(Basic_Null) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);
        const TString folderId = "some_folder_id";
        auto expectedStatus = YandexQuery::QueryMeta::COMPLETED;
        CreateNewHistoryAndWaitFinish(folderId, client, "select null", expectedStatus);
    }

    SIMPLE_UNIT_FORKED_TEST(Basic_Tagged) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);
        const TString folderId = "some_folder_id";


        {
            auto request = ::NYq::TCreateConnectionBuilder{}
                                .SetName("testdb00")
                                .CreateYdb("Root", location, "")
                                .Build();

            auto result = client.CreateConnection(
                request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();

            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
        }

        auto expectedStatus = YandexQuery::QueryMeta::COMPLETED;
        CreateNewHistoryAndWaitFinish(folderId, client, "select AsTagged(count(*), \"tag\") from testdb00.`yq/connections`", expectedStatus);
    }

    Y_UNIT_TEST(Basic_TaggedLiteral) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);
        const TString folderId = "some_folder_id";

        auto expectedStatus = YandexQuery::QueryMeta::COMPLETED;
        CreateNewHistoryAndWaitFinish(folderId, client, "select AsTagged(1, \"tag\")", expectedStatus);
    }

    // use fork for data test due to ch initialization problem
    SIMPLE_UNIT_FORKED_TEST(ExtendedDatabaseId) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));

        NYdb::NYq::TClient client(driver);
        const TString folderId = "folder_id_" + CreateGuidAsString();
        {
            const auto request = ::NYq::TCreateConnectionBuilder()
                .SetName("testdb01")
                .CreateYdb("FakeDatabaseId", "")
                .Build();
            const auto result = client
                .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
        }

        {
            const auto request = ::NYq::TCreateConnectionBuilder()
                .SetName("testdb02")
                .CreateYdb("FakeDatabaseId", "")
                .Build();
            const auto result = client
                .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
        }

        {
            const auto queryId = CreateNewHistoryAndWaitFinish(folderId, client,
                "select count(*) from testdb01.`yq/connections`", YandexQuery::QueryMeta::COMPLETED);
            CheckGetResultData(client, queryId, folderId, 1, 1, 2);
        }

        {
            // test connections db with 2 databaseId
            const auto queryId = CreateNewHistoryAndWaitFinish(folderId, client,
                "select count(*) from testdb02.`yq/connections`", YandexQuery::QueryMeta::COMPLETED);
            CheckGetResultData(client, queryId, folderId, 1, 1, 2);
        }
    }

    Y_UNIT_TEST(DescribeConnection) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);
        const TString folderId = "some_folder_id";
        TString conId;
        {
            const auto request = ::NYq::TCreateConnectionBuilder()
                .SetName("created_conn")
                .CreateYdb("created_db", "")
                .Build();
            const auto result = client
                .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
            conId = result.GetResult().connection_id();
        }
        {
            auto request = ::NYq::TDescribeConnectionBuilder()
                .SetConnectionId(conId)
                .Build();
            auto result = client
                .DescribeConnection(request, CreateYqSettings<TDescribeConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL(result.GetStatus(), EStatus::SUCCESS);
            const auto& res = result.GetResult().connection();
            UNIT_ASSERT_VALUES_EQUAL(res.meta().id(), conId);
            UNIT_ASSERT_VALUES_EQUAL(res.meta().created_by(), "root@builtin");
            UNIT_ASSERT_VALUES_EQUAL(res.meta().modified_by(), "root@builtin");
            UNIT_ASSERT_VALUES_EQUAL(res.content().name(), "created_conn");
        }
    }

    Y_UNIT_TEST(ListConnections) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);
        const size_t conns = 3;
        const auto folderId = TString(__func__) + "folder_id";
        {//CreateConnections
            for (size_t i = 0; i < conns - 1; ++i) {
                const auto request = ::NYq::TCreateConnectionBuilder()
                    .SetName("testdb" + ToString(i))
                    .CreateYdb("FakeDatabaseId", "")
                    .Build();
                const auto result = client
                    .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                    .ExtractValueSync();
                UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
            }

            // yds
            const auto request = ::NYq::TCreateConnectionBuilder()
                .SetName("testdb2")
                .CreateDataStreams("FakeDatabaseId", "") // We can use the same db in yds and ydb
                .Build();
            const auto result = client
                .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
        }

        {
            const auto request = ::NYq::TListConnectionsBuilder().Build();
            auto result = client
                .ListConnections(request, CreateYqSettings<TListConnectionsSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            UNIT_ASSERT_VALUES_EQUAL(result.GetResult().connection().size(), conns);
            size_t i = 0;
            auto res = result.GetResult();
            auto* conns = res.mutable_connection();
            std::sort(conns->begin(), conns->end(), [&](const auto& lhs, const auto& rhs) {
                return lhs.content().name() < rhs.content().name();
            });
            for (const auto& conn : *conns) {
                const auto& content = conn.content();
                const auto& meta = conn.meta();
                UNIT_ASSERT_VALUES_EQUAL(content.name(), "testdb" + ToString(i));
                UNIT_ASSERT_VALUES_EQUAL(meta.created_by(), "root@builtin");
                UNIT_ASSERT_VALUES_EQUAL(meta.modified_by(), "root@builtin");
                if (i < 2) {
                    UNIT_ASSERT_C(content.setting().has_ydb_database(), content);
                } else {
                    UNIT_ASSERT_C(content.setting().has_data_streams(), content);
                }
                i++;
            }
        }
    }

    Y_UNIT_TEST(ListConnectionsOnEmptyConnectionsTable) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);

        {
            const auto request = ::NYq::TListConnectionsBuilder().Build();
            auto result = client
                .ListConnections(request, CreateYqSettings<TListConnectionsSettings>("WTF"))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            UNIT_ASSERT(result.GetResult().connection().empty());
        }
    }

    Y_UNIT_TEST(ModifyConnections) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        TString userToken = "root@builtin";
        TString userId = "root";
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken(userToken));
        NYdb::NYq::TClient client(driver);
        const auto folderId = TString(__func__) + "folder_id";
        TString conId;

        {
            const auto request = ::NYq::TCreateConnectionBuilder()
                .SetName("created_conn")
                .CreateYdb("created_db", "")
                .Build();
            const auto result = client
                .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
            conId = result.GetResult().connection_id();
        }

        {//Modify
            const auto request = ::NYq::TModifyConnectionBuilder()
                .SetName("modified_name")
                .SetConnectionId(conId)
                .CreateYdb("new ydb", "")
                .SetDescription("Modified")
                .Build();
            const auto result = client
                .ModifyConnection(request, CreateYqSettings<TModifyConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
        }

        {
            auto request = ::NYq::TDescribeConnectionBuilder()
                .SetConnectionId(conId)
                .Build();
            auto result = client
                .DescribeConnection(request, CreateYqSettings<TDescribeConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            const auto& res = result.GetResult().connection();
            UNIT_ASSERT_VALUES_EQUAL(res.meta().id(), conId);
            UNIT_ASSERT_VALUES_EQUAL(res.meta().created_by(), "root@builtin");
            UNIT_ASSERT_VALUES_EQUAL(res.meta().modified_by(), "root@builtin");
            UNIT_ASSERT_VALUES_EQUAL(res.content().name(), "modified_name");
            UNIT_ASSERT_VALUES_EQUAL(res.content().description(), "Modified");
        }
    }

    Y_UNIT_TEST(DeleteConnections) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);
        const auto folderId = TString(__func__) + "folder_id";
        TString conId;

        {
            const auto request = ::NYq::TCreateConnectionBuilder()
                .SetName("created_conn")
                .CreateYdb("created_db", "")
                .Build();
            const auto result = client
                .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
            conId = result.GetResult().connection_id();
        }

        {
            const auto request = ::NYq::TDeleteConnectionBuilder()
                .SetConnectionId(conId)
                .Build();

            const auto result = client
                .DeleteConnection(request, CreateYqSettings<TDeleteConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
        }
    }

    Y_UNIT_TEST(Create_And_Modify_The_Same_Connection) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        TString userToken = "root@builtin";
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken(userToken));
        NYdb::NYq::TClient client(driver);

        const auto folderId = TString(__func__) + "folder_id";
        TString conId;

        {
            const auto request = ::NYq::TCreateConnectionBuilder()
                .SetName("created_conn")
                .CreateYdb("created_db", "")
                .Build();
            const auto result = client
                .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
            conId = result.GetResult().connection_id();
        }

        {
            const auto request = ::NYq::TModifyConnectionBuilder()
                .SetConnectionId(conId)
                .CreateYdb("modified_db", "")//TODO remove
                .Build();
            const auto result = client
                .ModifyConnection(request, CreateYqSettings<TModifyConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
        }
    }

    Y_UNIT_TEST(CreateConnection_With_Existing_Name) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        TString userToken = "root@builtin";
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken(userToken));
        NYdb::NYq::TClient client(driver);

        const auto folderId = TString(__func__) + "folder_id";
        auto name = TString(__func__) + "_name";
        name.to_lower();

        {
            const auto request = ::NYq::TCreateConnectionBuilder()
                .SetName(name)
                .CreateYdb("created_db", "")
                .Build();
            const auto result = client
                .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
        }

        {
            const auto request = ::NYq::TCreateConnectionBuilder()
                .SetName(name)
                .CreateYdb("created_db", "")
                .Build();
            const auto result = client
                .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::BAD_REQUEST, result.GetIssues().ToString()); //TODO status should be ALREADY_EXISTS
        }
    }

    Y_UNIT_TEST(CreateConnections_With_Idempotency) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        TString userToken = "root@builtin";
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken(userToken));
        NYdb::NYq::TClient client(driver);

        const auto folderId = TString(__func__) + "folder_id";
        const auto name = "connection_name";
        const TString idempotencyKey = "idempotency_key";
        TString conId;

        {
            const auto request = ::NYq::TCreateConnectionBuilder()
                .SetName(name)
                .SetIdempotencyKey(idempotencyKey)
                .CreateYdb("created_db", "")
                .Build();
            const auto result = client
                .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
            conId = result.GetResult().connection_id();
        }

        {
            const auto request = ::NYq::TCreateConnectionBuilder()
                .SetName(name)
                .SetIdempotencyKey(idempotencyKey)
                .CreateYdb("created_db", "")
                .Build();
            const auto result = client
                .CreateConnection(request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_C(result.GetStatus() == EStatus::SUCCESS, result.GetIssues().ToString());
            UNIT_ASSERT_VALUES_EQUAL(conId, result.GetResult().connection_id());
        }
    }

    Y_UNIT_TEST(CreateQuery_With_Idempotency) {//TODO Fix
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        TString userToken = "root@builtin";
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken(userToken));
        NYdb::NYq::TClient client(driver);

        const auto folderId = TString(__func__) + "folder_id";
        const TString idempotencyKey = "idempotency_key";
        const TString yqlText  = "select 1";
        TString queryId;
        const auto request = ::NYq::TCreateQueryBuilder{}
            .SetText(yqlText)
            .SetIdempotencyKey(idempotencyKey)
            .Build();

        {
            auto result = client.CreateQuery(
                request, CreateYqSettings<TCreateQuerySettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            queryId = result.GetResult().query_id();
        }

        {
            const auto req = ::NYq::TDescribeQueryBuilder{}
                .SetQueryId(queryId)
                .Build();
            const auto result = DoWithRetryOnRetCode([&]() {
                auto result = client.DescribeQuery(
                    req, CreateYqSettings<TDescribeQuerySettings>(folderId))
                    .ExtractValueSync();
                UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
                const auto status = result.GetResult().query().meta().status();
                PrintProtoIssues(result.GetResult().query().issue());
                return status == YandexQuery::QueryMeta::COMPLETED;
            }, TRetryOptions(Retries));
            UNIT_ASSERT_C(result, "the execution of the query did not end within the time limit");
        }
        {
            auto result = client.CreateQuery(
                request, CreateYqSettings<TCreateQuerySettings>(folderId))
                .ExtractValueSync();

            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            UNIT_ASSERT_VALUES_EQUAL(queryId, result.GetResult().query_id());
        }
        CheckGetResultData(client, queryId, folderId, 1, 1, 1);
    }

    // use fork for data test due to ch initialization problem
    SIMPLE_UNIT_FORKED_TEST(CreateQuery_Without_Connection) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        TString userToken = "root@builtin";
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken(userToken));
        NYdb::NYq::TClient client(driver);

        const TString yqlText  = "select count(*) from testdbWTF.`connections`";
        CreateNewHistoryAndWaitFinish("folder_id_WTF", client,
            yqlText, YandexQuery::QueryMeta::FAILED);
    }

    Y_UNIT_TEST(DeleteQuery) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        TString userToken = "root@builtin";
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken(userToken));
        NYdb::NYq::TClient client(driver);

        const auto folderId = TString(__func__) + "folder_id";
        const TString yqlText = "select 1";
        const TString queryId = CreateNewHistoryAndWaitFinish(folderId, client,
            yqlText, YandexQuery::QueryMeta::COMPLETED);
        CheckGetResultData(client, queryId, folderId, 1, 1, 1);

        {
            const auto request = ::NYq::TDeleteQueryBuilder()
                .SetQueryId(queryId)
                .Build();
            auto result = client
                .DeleteQuery(request, CreateYqSettings<TDeleteQuerySettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL(result.IsTransportError(), false);
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
        }

        {
            auto request = ::NYq::TDescribeQueryBuilder()
                .SetQueryId(queryId)
                .Build();
            auto result = client
                .DescribeQuery(request, CreateYqSettings<TDescribeQuerySettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::BAD_REQUEST, result.GetIssues().ToString());
        }
    }

    Y_UNIT_TEST(ModifyQuery) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        TString userToken = "root@builtin";
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken(userToken));
        NYdb::NYq::TClient client(driver);

        const auto folderId = TString(__func__) + "folder_id";
        const TString yqlText = "select 1";
        const TString queryId = CreateNewHistoryAndWaitFinish(folderId, client,
            yqlText, YandexQuery::QueryMeta::COMPLETED);
        CheckGetResultData(client, queryId, folderId, 1, 1, 1);

        {
            const auto request = ::NYq::TModifyQueryBuilder()
                .SetQueryId(queryId)
                .SetName("ModifiedName")
                .SetDescription("OK")
                .Build();
            auto result = client
                .ModifyQuery(request, CreateYqSettings<TModifyQuerySettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL(result.IsTransportError(), false);
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
        }

        {
            auto request = ::NYq::TDescribeQueryBuilder()
                .SetQueryId(queryId)
                .Build();
            auto result = client
                .DescribeQuery(request, CreateYqSettings<TDescribeQuerySettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            const auto& query = result.GetResult().query();
            UNIT_ASSERT_VALUES_EQUAL(query.content().name(), "ModifiedName");
            UNIT_ASSERT_VALUES_EQUAL(query.content().description(), "OK");
        }
    }

    Y_UNIT_TEST(DescribeJob) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc        = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver      = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);
        const auto folderId = "some_folder_id";
        const auto queryId = CreateNewHistoryAndWaitFinish(folderId, client, "select 1", YandexQuery::QueryMeta::COMPLETED);
        CheckGetResultData(client, queryId, folderId, 1, 1, 1);
        TString jobId;

        {
            auto request = ::NYq::TListJobsBuilder{}.SetQueryId(queryId).Build();
            auto result = DoWithRetryOnRetCode([&]() {
                auto result = client.ListJobs(
                    request, CreateYqSettings<TListJobsSettings>(folderId))
                    .ExtractValueSync();
                UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
                UNIT_ASSERT_VALUES_EQUAL(result.GetResult().job_size(), 1);
                jobId = result.GetResult().job(0).meta().id();
                return result.GetStatus() == EStatus::SUCCESS;
            }, TRetryOptions(Retries));
            UNIT_ASSERT_C(result, "the execution of the query did not end within the time limit");
        }

        {
            const auto request = ::NYq::TDescribeJobBuilder()
                .SetJobId(jobId)
                .Build();
            auto result = client.DescribeJob(
                request, CreateYqSettings<TDescribeJobSettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            UNIT_ASSERT_VALUES_EQUAL(result.GetResult().job().query_meta().common().id(), queryId);
            UNIT_ASSERT_VALUES_EQUAL(result.GetResult().job().meta().id(), jobId);
            UNIT_ASSERT_VALUES_EQUAL(result.GetResult().job().query_name(), "test_query_name_1");
        }
    }

    Y_UNIT_TEST(DescribeQuery) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc        = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver      = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);
        const auto folderId = "some_folder_id";
        const auto queryId = CreateNewHistoryAndWaitFinish(folderId, client, "select 1", YandexQuery::QueryMeta::COMPLETED);
        CheckGetResultData(client, queryId, folderId, 1, 1, 1);
        TString jobId;

        {
            const auto request = ::NYq::TDescribeQueryBuilder()
                .SetQueryId(queryId)
                .Build();
            auto result = client.DescribeQuery(
                request, CreateYqSettings<TDescribeQuerySettings>(folderId))
                .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            const auto query = result.GetResult().query();
            UNIT_ASSERT_VALUES_EQUAL(YandexQuery::QueryMeta::ComputeStatus_Name(query.meta().status()), YandexQuery::QueryMeta::ComputeStatus_Name(YandexQuery::QueryMeta::COMPLETED));
            UNIT_ASSERT_VALUES_EQUAL(query.content().text(), "select 1");
            UNIT_ASSERT_VALUES_EQUAL(query.content().name(), "test_query_name_1");
        }
    }
}

Y_UNIT_TEST_SUITE(Yq_2) {
    // use fork for data test due to ch initialization problem
    Y_UNIT_TEST(Test_HostNameTrasformation) {
        UNIT_ASSERT_VALUES_EQUAL(::NYq::TransformMdbHostToCorrectFormat("rc1c-p5waby2y5y1kb5ue.mdb.yandexcloud.net"), "rc1c-p5waby2y5y1kb5ue.db.yandex.net:8443");
        UNIT_ASSERT_VALUES_EQUAL(::NYq::TransformMdbHostToCorrectFormat("xxx.xxx"), "xxx.db.yandex.net:8443");
        UNIT_ASSERT_VALUES_EQUAL(::NYq::TransformMdbHostToCorrectFormat("host."), "host.db.yandex.net:8443");
    }

    SIMPLE_UNIT_FORKED_TEST(ReadFromYdbOverYq) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc        = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver      = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        NYdb::NYq::TClient client(driver);
        const auto folderId = TString(__func__) + "folder_id";

        {
            auto request = ::NYq::TCreateConnectionBuilder{}
                                .SetName("testdb00")
                                .CreateYdb("Root", location, "")
                                .Build();

            auto result = client.CreateConnection(
                request, CreateYqSettings<TCreateConnectionSettings>(folderId))
                .ExtractValueSync();

            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
        }

        TString queryId;
        {
            auto request = ::NYq::TCreateQueryBuilder{}
                                .SetText("select count(*) from testdb00.`yq/connections`")
                                .Build();
            auto result = client.CreateQuery(
                request, CreateYqSettings<TCreateQuerySettings>(folderId))
                .ExtractValueSync();

            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
            queryId = result.GetResult().query_id();
        }

        {
            auto request = ::NYq::TDescribeQueryBuilder{}.SetQueryId(queryId).Build();
            auto result = DoWithRetryOnRetCode([&]() {
                auto result = client.DescribeQuery(
                    request, CreateYqSettings<TDescribeQuerySettings>(folderId))
                    .ExtractValueSync();
                UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());
                const auto status = result.GetResult().query().meta().status();
                PrintProtoIssues(result.GetResult().query().issue());
                return status == YandexQuery::QueryMeta::COMPLETED;
            }, TRetryOptions(10));
            UNIT_ASSERT_C(result, "the execution of the query did not end within the time limit");
        }

        {
            auto request = ::NYq::TGetResultDataBuilder{}.SetQueryId(queryId).Build();
            auto result = client.GetResultData(
                    request, CreateYqSettings<TGetResultDataSettings>(folderId))
                    .ExtractValueSync();
            UNIT_ASSERT_VALUES_EQUAL_C(result.GetStatus(), EStatus::SUCCESS, result.GetIssues().ToString());

            const auto& resultSet = result.GetResult().result_set();
            UNIT_ASSERT_VALUES_EQUAL(resultSet.rows().size(), 1);
            UNIT_ASSERT_VALUES_EQUAL(resultSet.columns().size(), 1);
            UNIT_ASSERT_VALUES_EQUAL(resultSet.rows(0).items(0).uint64_value(), 1);
        }
    }
}

Y_UNIT_TEST_SUITE(PrivateApi) {
    Y_UNIT_TEST(PingTask) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        ::NFq::TPrivateClient client(driver);
        const TString historyId = "id";
        const TString folderId = "folder_id";
        const TScope scope(folderId);
        {
            Fq::Private::PingTaskRequest req;
            req.mutable_query_id()->set_value("id");
            req.set_scope(scope.ToString());
            req.set_owner_id("some_owner");
            req.set_status(YandexQuery::QueryMeta::COMPLETED);
            auto result = client.PingTask(std::move(req)).ExtractValueSync();
            result.GetIssues().PrintTo(Cerr);
            UNIT_ASSERT_VALUES_EQUAL(result.GetStatus(), EStatus::GENERIC_ERROR);
        }
    }

    Y_UNIT_TEST(GetTask) {//PendingFetcher can take task first
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        ::NFq::TPrivateClient client(driver);
        {
            Fq::Private::GetTaskRequest req;
            req.set_owner_id("owner_id");
            req.set_host("host");
            auto result = client.GetTask(std::move(req)).ExtractValueSync();
            result.GetIssues().PrintTo(Cerr);
            UNIT_ASSERT_VALUES_EQUAL(result.GetStatus(), EStatus::SUCCESS);
            result.GetIssues().PrintTo(Cerr);
        }
    }

    Y_UNIT_TEST(Nodes) {
        TKikimrWithGrpcAndRootSchema server({}, {}, {}, true);
        ui16 grpc = server.GetPort();
        TString location = TStringBuilder() << "localhost:" << grpc;
        auto driver = TDriver(TDriverConfig().SetEndpoint(location).SetAuthToken("root@builtin"));
        ::NFq::TPrivateClient client(driver);
        const auto instanceId = CreateGuidAsString();
        {
            Fq::Private::NodesHealthCheckRequest req;
            req.set_tenant("Tenant");
            auto& node = *req.mutable_node();
            node.set_hostname("hostname");
            node.set_node_id(100500);
            node.set_instance_id(instanceId);
            const auto result = DoWithRetryOnRetCode([&]() {
                auto r = req;
                auto result = client.NodesHealthCheck(std::move(r)).ExtractValueSync();
                if (result.GetStatus() == EStatus::SUCCESS) {
                    const auto& res = result.GetResult();
                    UNIT_ASSERT(!res.nodes().empty());
                    UNIT_ASSERT_VALUES_EQUAL(res.nodes(0).hostname(), "hostname");
                    UNIT_ASSERT_VALUES_EQUAL(res.nodes(0).node_id(), 100500);
                    UNIT_ASSERT_VALUES_EQUAL(res.nodes(0).instance_id(), instanceId);
                }
                // result.GetIssues().PrintTo(Cerr);
                return result.GetStatus() == EStatus::SUCCESS;
            }, TRetryOptions(Retries));
            UNIT_ASSERT_C(result, "the execution of the query did not end within the time limit");
        }
    }
}
