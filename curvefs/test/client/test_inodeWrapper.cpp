
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
 * Project: curve
 * Created Date: 2021-12-28
 * Author: xuchaojie
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <google/protobuf/util/message_differencer.h>

#include "curvefs/test/client/mock_metaserver_client.h"
#include "curvefs/src/client/inode_wrapper.h"

using ::google::protobuf::util::MessageDifferencer;

namespace curvefs {
namespace client {

using ::testing::Return;
using ::testing::_;
using ::testing::Contains;
using ::testing::SetArgPointee;
using ::testing::SetArgReferee;
using ::testing::DoAll;

using rpcclient::MockMetaServerClient;

class TestInodeWrapper : public ::testing::Test {
 protected:
    TestInodeWrapper() {}
    ~TestInodeWrapper() {}

    virtual void SetUp() {
        metaClient_ = std::make_shared<MockMetaServerClient>();
        inodeWrapper_ = std::make_shared<InodeWrapper>(Inode(), metaClient_);
    }

    virtual void TearDown() {
        metaClient_ = nullptr;
        inodeWrapper_ = nullptr;
    }

 protected:
    std::shared_ptr<InodeWrapper> inodeWrapper_;
    std::shared_ptr<MockMetaServerClient> metaClient_;
};

TEST(TestAppendS3ChunkInfoToMap, testAppendS3ChunkInfoToMap) {
    google::protobuf::Map<uint64_t, S3ChunkInfoList> s3ChunkInfoMap;
    S3ChunkInfo info1;
    info1.set_chunkid(1);
    info1.set_compaction(2);
    info1.set_offset(0);
    info1.set_len(1024);
    info1.set_size(65536);
    info1.set_size(true);
    uint64_t chunkIndex1 = 1;
    AppendS3ChunkInfoToMap(chunkIndex1, info1, &s3ChunkInfoMap);
    ASSERT_EQ(1, s3ChunkInfoMap.size());
    ASSERT_EQ(1, s3ChunkInfoMap[chunkIndex1].s3chunks_size());
    ASSERT_TRUE(MessageDifferencer::Equals(info1,
        s3ChunkInfoMap[chunkIndex1].s3chunks(0)));


    // add to same chunkIndex
    S3ChunkInfo info2;
    info2.set_chunkid(2);
    info2.set_compaction(3);
    info2.set_offset(1024);
    info2.set_len(1024);
    info2.set_size(65536);
    info2.set_size(false);
    AppendS3ChunkInfoToMap(chunkIndex1, info2, &s3ChunkInfoMap);
    ASSERT_EQ(1, s3ChunkInfoMap.size());
    ASSERT_EQ(2, s3ChunkInfoMap[chunkIndex1].s3chunks_size());
    ASSERT_TRUE(MessageDifferencer::Equals(info1,
        s3ChunkInfoMap[chunkIndex1].s3chunks(0)));
    ASSERT_TRUE(MessageDifferencer::Equals(info2,
        s3ChunkInfoMap[chunkIndex1].s3chunks(1)));

    // add to diff chunkIndex
    S3ChunkInfo info3;
    info3.set_chunkid(3);
    info3.set_compaction(4);
    info3.set_offset(2048);
    info3.set_len(1024);
    info3.set_size(65536);
    info3.set_size(false);
    uint64_t chunkIndex2 = 2;
    AppendS3ChunkInfoToMap(chunkIndex2, info3, &s3ChunkInfoMap);
    ASSERT_EQ(2, s3ChunkInfoMap.size());
    ASSERT_EQ(2, s3ChunkInfoMap[chunkIndex1].s3chunks_size());
    ASSERT_TRUE(MessageDifferencer::Equals(info1,
        s3ChunkInfoMap[chunkIndex1].s3chunks(0)));
    ASSERT_TRUE(MessageDifferencer::Equals(info2,
        s3ChunkInfoMap[chunkIndex1].s3chunks(1)));

    ASSERT_EQ(1, s3ChunkInfoMap[chunkIndex2].s3chunks_size());
    ASSERT_TRUE(MessageDifferencer::Equals(info3,
        s3ChunkInfoMap[chunkIndex2].s3chunks(0)));
}

TEST_F(TestInodeWrapper, testSyncSuccess) {
    inodeWrapper_->MarkDirty();
    inodeWrapper_->SetLength(1024);

    S3ChunkInfo info1;
    info1.set_chunkid(1);
    info1.set_compaction(2);
    info1.set_offset(0);
    info1.set_len(1024);
    info1.set_size(65536);
    info1.set_size(true);
    uint64_t chunkIndex1 = 1;
    inodeWrapper_->AppendS3ChunkInfo(chunkIndex1, info1);

    EXPECT_CALL(*metaClient_, UpdateInode(_))
        .WillOnce(Return(MetaStatusCode::OK));

    EXPECT_CALL(*metaClient_, GetOrModifyS3ChunkInfo(_, _, _, _, _))
        .WillOnce(Return(MetaStatusCode::OK));

    CURVEFS_ERROR ret = inodeWrapper_->Sync();
    ASSERT_EQ(CURVEFS_ERROR::OK, ret);
}

TEST_F(TestInodeWrapper, testSyncFailed) {
    inodeWrapper_->MarkDirty();
    inodeWrapper_->SetLength(1024);

    S3ChunkInfo info1;
    info1.set_chunkid(1);
    info1.set_compaction(2);
    info1.set_offset(0);
    info1.set_len(1024);
    info1.set_size(65536);
    info1.set_size(true);
    uint64_t chunkIndex1 = 1;
    inodeWrapper_->AppendS3ChunkInfo(chunkIndex1, info1);

    EXPECT_CALL(*metaClient_, UpdateInode(_))
        .WillOnce(Return(MetaStatusCode::NOT_FOUND))
        .WillOnce(Return(MetaStatusCode::OK));

    EXPECT_CALL(*metaClient_, GetOrModifyS3ChunkInfo(_, _, _, _, _))
        .WillOnce(Return(MetaStatusCode::NOT_FOUND));

    CURVEFS_ERROR ret = inodeWrapper_->Sync();
    ASSERT_EQ(CURVEFS_ERROR::NOTEXIST, ret);

    CURVEFS_ERROR ret2 = inodeWrapper_->Sync();
    ASSERT_EQ(CURVEFS_ERROR::NOTEXIST, ret2);
}


}  // namespace client
}  // namespace curvefs
