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
 * @Date: 2021-06-10 10:04:57
 * @Author: chenwei
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <google/protobuf/util/message_differencer.h>

#include "curvefs/test/metaserver/test_helper.h"
#include "curvefs/src/metaserver/inode_manager.h"

using ::testing::AtLeast;
using ::testing::StrEq;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::SaveArg;
using ::google::protobuf::util::MessageDifferencer;

namespace curvefs {
namespace metaserver {
class InodeManagerTest : public ::testing::Test {
 protected:
    void SetUp() override { return; }

    void TearDown() override { return; }

    bool CompareInode(const Inode &first, const Inode &second) {
        return first.fsid() == second.fsid() &&
               first.atime() == second.atime() &&
               first.inodeid() == second.inodeid() &&
               first.length() == second.length() &&
               first.uid() == second.uid() && first.gid() == second.gid() &&
               first.mode() == second.mode() && first.type() == second.type() &&
               first.mtime() == second.mtime() &&
               first.ctime() == second.ctime() &&
               first.symlink() == second.symlink() &&
               first.nlink() == second.nlink();
    }
};

TEST_F(InodeManagerTest, test1) {
    std::shared_ptr<InodeStorage> inodeStorage =
        std::make_shared<MemoryInodeStorage>();
    auto trash = std::make_shared<TrashImpl>(inodeStorage);
    InodeManager manager(inodeStorage, trash);

    // CREATE
    uint32_t fsId = 1;
    uint64_t length = 100;
    uint32_t uid = 200;
    uint32_t gid = 300;
    uint32_t mode = 400;
    FsFileType type = FsFileType::TYPE_FILE;
    std::string symlink = "";
    Inode inode1;
    ASSERT_EQ(manager.CreateInode(fsId, 2, length, uid, gid, mode, type,
        symlink, 0, &inode1),
        MetaStatusCode::OK);
    ASSERT_EQ(inode1.inodeid(), 2);

    Inode inode2;
    ASSERT_EQ(manager.CreateInode(fsId, 3, length, uid, gid, mode, type,
        symlink, 0, &inode2),
        MetaStatusCode::OK);
    ASSERT_EQ(inode2.inodeid(), 3);

    Inode inode3;
    ASSERT_EQ(manager.CreateInode(fsId, 4, length, uid, gid, mode,
        FsFileType::TYPE_SYM_LINK, symlink, 0, &inode3),
        MetaStatusCode::SYM_LINK_EMPTY);

    ASSERT_EQ(
        manager.CreateInode(fsId, 4, length, uid, gid, mode,
        FsFileType::TYPE_SYM_LINK, "SYMLINK", 0, &inode3),
        MetaStatusCode::OK);
    ASSERT_EQ(inode3.inodeid(), 4);

    Inode inode4;
    ASSERT_EQ(manager.CreateInode(fsId, 5, length, uid, gid, mode,
        FsFileType::TYPE_S3, symlink, 0, &inode4),
        MetaStatusCode::OK);
    ASSERT_EQ(inode4.inodeid(), 5);
    ASSERT_EQ(inode4.type(), FsFileType::TYPE_S3);

    // GET
    Inode temp1;
    ASSERT_EQ(manager.GetInode(fsId, inode1.inodeid(), &temp1),
              MetaStatusCode::OK);
    ASSERT_TRUE(CompareInode(inode1, temp1));

    Inode temp2;
    ASSERT_EQ(manager.GetInode(fsId, inode2.inodeid(), &temp2),
              MetaStatusCode::OK);
    ASSERT_TRUE(CompareInode(inode2, temp2));

    Inode temp3;
    ASSERT_EQ(manager.GetInode(fsId, inode3.inodeid(), &temp3),
              MetaStatusCode::OK);
    ASSERT_TRUE(CompareInode(inode3, temp3));

    Inode temp4;
    ASSERT_EQ(manager.GetInode(fsId, inode4.inodeid(), &temp4),
              MetaStatusCode::OK);
    ASSERT_TRUE(CompareInode(inode4, temp4));

    // DELETE
    ASSERT_EQ(manager.DeleteInode(fsId, inode1.inodeid()), MetaStatusCode::OK);
    ASSERT_EQ(manager.DeleteInode(fsId, inode1.inodeid()),
              MetaStatusCode::NOT_FOUND);
    ASSERT_EQ(manager.GetInode(fsId, inode1.inodeid(), &temp1),
              MetaStatusCode::NOT_FOUND);

    // UPDATE
    UpdateInodeRequest request = MakeUpdateInodeRequestFromInode(inode1);
    ASSERT_EQ(manager.UpdateInode(request), MetaStatusCode::NOT_FOUND);
    temp2.set_atime(100);
    UpdateInodeRequest request2 = MakeUpdateInodeRequestFromInode(temp2);
    ASSERT_EQ(manager.UpdateInode(request2), MetaStatusCode::OK);
    Inode temp5;
    ASSERT_EQ(manager.GetInode(fsId, inode2.inodeid(), &temp5),
              MetaStatusCode::OK);
    ASSERT_TRUE(CompareInode(temp5, temp2));
    ASSERT_FALSE(CompareInode(inode2, temp2));

    // GetOrModifyS3ChunkInfo
    google::protobuf::Map<uint64_t, S3ChunkInfoList> s3ChunkInfoAdd;
    google::protobuf::Map<uint64_t, S3ChunkInfoList> s3ChunkInfoRemove;

    S3ChunkInfo info[100];
    for (int i = 0; i < 100; i++) {
        info[i].set_chunkid(i);
        info[i].set_compaction(i);
        info[i].set_offset(i);
        info[i].set_len(i);
        info[i].set_size(i);
        info[i].set_zero(true);
    }

    S3ChunkInfoList list[10];
    for (int j = 0; j < 10; j++) {
        for (int k = 0; k < 10; k++) {
            S3ChunkInfo *tmp = list[j].add_s3chunks();
            tmp->CopyFrom(info[10 * j + k]);
        }
    }

    for (int j = 0; j < 10; j++) {
        s3ChunkInfoAdd[j] = list[j];
    }

    google::protobuf::Map<
            uint64_t, S3ChunkInfoList> s3Out1;
    ASSERT_EQ(MetaStatusCode::OK,
        manager.GetOrModifyS3ChunkInfo(
            fsId, inode3.inodeid(), s3ChunkInfoAdd, s3ChunkInfoRemove,
            true, &s3Out1, false));

    ASSERT_EQ(10, s3Out1.size());
    for (int j = 0; j < 10; j++) {
        ASSERT_TRUE(MessageDifferencer::Equals(s3ChunkInfoAdd[j],
                s3Out1.at(j)));
    }

    // Idempotent test
    google::protobuf::Map<
            uint64_t, S3ChunkInfoList> s3Out2;
    ASSERT_EQ(MetaStatusCode::OK,
        manager.GetOrModifyS3ChunkInfo(
            fsId, inode3.inodeid(), s3ChunkInfoAdd, s3ChunkInfoRemove,
            true, &s3Out2, false));

    ASSERT_EQ(10, s3Out2.size());
    for (int j = 0; j < 10; j++) {
        ASSERT_TRUE(MessageDifferencer::Equals(s3ChunkInfoAdd[j],
                s3Out2.at(j)));
    }

    google::protobuf::Map<
            uint64_t, S3ChunkInfoList> s3Out3;
    ASSERT_EQ(MetaStatusCode::OK,
        manager.GetOrModifyS3ChunkInfo(
            fsId, inode3.inodeid(), s3ChunkInfoRemove, s3ChunkInfoAdd,
        true, &s3Out3, false));
    ASSERT_EQ(0, s3Out3.size());

    // Idempotent test
    google::protobuf::Map<
            uint64_t, S3ChunkInfoList> s3Out4;
    ASSERT_EQ(MetaStatusCode::OK,
        manager.GetOrModifyS3ChunkInfo(
            fsId, inode3.inodeid(), s3ChunkInfoRemove, s3ChunkInfoAdd,
            true, &s3Out4, false));
    ASSERT_EQ(0, s3Out4.size());

    // s3compact
    google::protobuf::Map<uint64_t, S3ChunkInfoList> addMap;
    google::protobuf::Map<uint64_t, S3ChunkInfoList> deleteMap;
    google::protobuf::Map<uint64_t, S3ChunkInfoList> s3Out5;
    S3ChunkInfoList add0;
    for (int i = 0; i < 10; i++) {
        S3ChunkInfo c;
        c.set_chunkid(i);
        c.set_compaction(0);
        c.set_offset(i);
        c.set_len(1);
        c.set_size(1);
        c.set_zero(true);
        *add0.add_s3chunks() = c;
    }
    S3ChunkInfoList add1;
    {
        S3ChunkInfo c;
        c.set_chunkid(7);
        c.set_compaction(1);
        c.set_offset(0);
        c.set_len(8);
        c.set_size(8);
        c.set_zero(true);
        *add1.add_s3chunks() = c;
    }
    S3ChunkInfoList delete1;
    for (int i = 0; i < 8; i++) {
        S3ChunkInfo c;
        c.set_chunkid(i);
        c.set_compaction(0);
        c.set_offset(i);
        c.set_len(1);
        c.set_size(1);
        c.set_zero(true);
        *delete1.add_s3chunks() = c;
    }
    addMap.insert({0, add0});
    ASSERT_EQ(MetaStatusCode::OK,
              manager.GetOrModifyS3ChunkInfo(fsId, inode3.inodeid(), addMap,
                                             deleteMap, true, &s3Out5, false));
    ASSERT_EQ(1, s3Out5.size());
    ASSERT_EQ(10, s3Out5.at(0).s3chunks_size());
    addMap.clear();
    addMap.insert({0, add1});
    deleteMap.insert({0, delete1});
    ASSERT_EQ(MetaStatusCode::OK,
              manager.GetOrModifyS3ChunkInfo(fsId, inode3.inodeid(), addMap,
                                             deleteMap, true, &s3Out5, true));
    ASSERT_EQ(1, s3Out5.size());
    ASSERT_EQ(3, s3Out5.at(0).s3chunks_size());
    ASSERT_EQ(7, s3Out5.at(0).s3chunks(0).chunkid());
    ASSERT_EQ(8, s3Out5.at(0).s3chunks(1).chunkid());
    ASSERT_EQ(9, s3Out5.at(0).s3chunks(2).chunkid());
}
}  // namespace metaserver
}  // namespace curvefs
