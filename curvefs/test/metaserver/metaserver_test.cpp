/*
 *  Copyright (c) 2021 NetEase Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * @Project: curve
 * @Date: 2021-06-10 10:46:50
 * @Author: chenwei
 */
#include <butil/at_exit.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>  // NOLINT

#include "curvefs/src/metaserver/metaserver.h"
#include "curvefs/test/metaserver/mock_topology_service.h"
#include "curvefs/test/metaserver/mock_heartbeat_service.h"

using ::testing::AtLeast;
using ::testing::StrEq;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::SaveArg;
using ::testing::Invoke;
using ::curvefs::mds::topology::MockTopologyService;
using ::curvefs::mds::topology::MetaServerRegistRequest;
using ::curvefs::mds::topology::MetaServerRegistResponse;
using ::curvefs::mds::topology::TopoStatusCode;
using ::curvefs::mds::heartbeat::MockHeartbeatService;
using ::curvefs::mds::heartbeat::MetaServerHeartbeatRequest;
using ::curvefs::mds::heartbeat::MetaServerHeartbeatResponse;
using ::curvefs::mds::heartbeat::HeartbeatStatusCode;

butil::AtExitManager atExit;

namespace curvefs {
namespace metaserver {
class MetaserverTest : public ::testing::Test {
 protected:
    void SetUp() override {
        metaserverIp_ = "127.0.0.1";
        metaserverPort_ = "56702";
        topologyServiceAddr_ = "127.0.0.1:56700";
        ASSERT_EQ(0, server_.AddService(&mockTopologyService_,
                                        brpc::SERVER_DOESNT_OWN_SERVICE));
        ASSERT_EQ(0, server_.AddService(&mockHeartbeatService_,
                                        brpc::SERVER_DOESNT_OWN_SERVICE));
        ASSERT_EQ(0, server_.Start(topologyServiceAddr_.c_str(), nullptr));
    }

    void TearDown() override {
        server_.Stop(0);
        server_.Join();
        return;
    }

    std::string metaserverIp_;
    std::string metaserverPort_;
    std::string topologyServiceAddr_;
    MockTopologyService mockTopologyService_;
    MockHeartbeatService mockHeartbeatService_;

    brpc::Server server_;
};

template <typename RpcRequestType, typename RpcResponseType,
          bool RpcFailed = false>
void RpcService(google::protobuf::RpcController *cntl_base,
                const RpcRequestType *request, RpcResponseType *response,
                google::protobuf::Closure *done) {
    if (RpcFailed) {
        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        cntl->SetFailed(112, "Not connected to");
    }
    done->Run();
}

TEST_F(MetaserverTest, register_to_mds_success) {
    curvefs::metaserver::Metaserver metaserver;
    auto conf = std::make_shared<Configuration>();
    conf->SetConfigPath("curvefs/conf/metaserver.conf");
    ASSERT_TRUE(conf->LoadConfig());
    conf->SetStringValue("mds.listen.addr", topologyServiceAddr_);
    conf->SetStringValue("global.ip", metaserverIp_);
    conf->SetStringValue("global.port", metaserverPort_);

    // initialize MDS options
    metaserver.InitOptions(conf);

    // mock RegistMetaServer
    MetaServerRegistResponse response;
    response.set_statuscode(TopoStatusCode::TOPO_OK);
    EXPECT_CALL(mockTopologyService_, RegistMetaServer(_, _, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(response),
                        Invoke(RpcService<MetaServerRegistRequest,
                                          MetaServerRegistResponse>)));

    // mock RegistMetaServer
    MetaServerHeartbeatResponse response1;
    response1.set_statuscode(HeartbeatStatusCode::hbOK);
    EXPECT_CALL(mockHeartbeatService_, MetaServerHeartbeat(_, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>(response1),
                        Invoke(RpcService<MetaServerHeartbeatRequest,
                                          MetaServerHeartbeatResponse>)));

    // Initialize other modules after winning election
    metaserver.Init();

    // start metaserver server
    std::thread metaserverThread(&Metaserver::Run, &metaserver);

    // sleep 2s
    sleep(2);

    // stop server and background threads
    metaserver.Stop();

    brpc::AskToQuit();
    metaserverThread.join();
}

TEST_F(MetaserverTest, test2) {
    curvefs::metaserver::Metaserver metaserver;
    auto conf = std::make_shared<Configuration>();
    conf->SetConfigPath("curvefs/conf/metaserver.conf");
    ASSERT_TRUE(conf->LoadConfig());
    conf->SetStringValue("mds.listen.addr", topologyServiceAddr_);
    conf->SetStringValue("global.ip", metaserverIp_);
    conf->SetStringValue("global.port", metaserverPort_);

    // mock RegistMetaServer
    MetaServerRegistResponse response;
    response.set_statuscode(TopoStatusCode::TOPO_OK);
    EXPECT_CALL(mockTopologyService_, RegistMetaServer(_, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>(response),
                              Invoke(RpcService<MetaServerRegistRequest,
                                                MetaServerRegistResponse>)));

    // mock RegistMetaServer
    MetaServerHeartbeatResponse response1;
    response1.set_statuscode(HeartbeatStatusCode::hbOK);
    EXPECT_CALL(mockHeartbeatService_, MetaServerHeartbeat(_, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>(response1),
                        Invoke(RpcService<MetaServerHeartbeatRequest,
                                          MetaServerHeartbeatResponse>)));

    // initialize Metaserver options
    metaserver.InitOptions(conf);

    // not init, run
    metaserver.Run();
    metaserver.Run();

    // not start, stop
    metaserver.Stop();
    metaserver.Stop();

    // Initialize other modules after winning election
    metaserver.Init();
    metaserver.Init();

    // start metaserver server
    std::thread metaserverThread(&Metaserver::Run, &metaserver);

    // sleep 2s
    sleep(2);

    // stop server and background threads
    metaserver.Stop();
    metaserverThread.join();
}
}  // namespace metaserver
}  // namespace curvefs
