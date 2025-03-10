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
 * Created Date: 2021-05-19
 * Author: chenwei
 */

#include "curvefs/src/metaserver/inode_manager.h"

#include <glog/logging.h>
#include <google/protobuf/util/message_differencer.h>
#include <list>

#include "curvefs/src/common/define.h"
#include "src/common/timeutility.h"

using ::curve::common::TimeUtility;
using ::curve::common::NameLockGuard;
using ::google::protobuf::util::MessageDifferencer;

namespace curvefs {
namespace metaserver {
MetaStatusCode InodeManager::CreateInode(uint32_t fsId, uint64_t inodeId,
                                         uint64_t length, uint32_t uid,
                                         uint32_t gid, uint32_t mode,
                                         FsFileType type,
                                         const std::string &symlink,
                                         uint64_t rdev,
                                         Inode *newInode) {
    VLOG(1) << "CreateInode, fsId = " << fsId << ", length = " << length
            << ", uid = " << uid << ", gid = " << gid << ", mode = " << mode
            << ", type =" << FsFileType_Name(type) << ", symlink = " << symlink
            << ", rdev = " << rdev;
    if (type == FsFileType::TYPE_SYM_LINK && symlink.empty()) {
        return MetaStatusCode::SYM_LINK_EMPTY;
    }

    // 1. generate inode
    Inode inode;
    GenerateInodeInternal(inodeId, fsId, length, uid, gid, mode, type, rdev,
                          &inode);
    if (type == FsFileType::TYPE_SYM_LINK) {
        inode.set_symlink(symlink);
    }

    // 2. insert inode
    MetaStatusCode ret = inodeStorage_->Insert(inode);
    if (ret != MetaStatusCode::OK) {
        LOG(ERROR) << "CreateInode fail, fsId = " << fsId
                   << ", length = " << length << ", uid = " << uid
                   << ", gid = " << gid << ", mode = " << mode
                   << ", type =" << FsFileType_Name(type)
                   << ", symlink = " << symlink
                   << ", ret = " << MetaStatusCode_Name(ret);
        return ret;
    }

    newInode->CopyFrom(inode);
    VLOG(1) << "CreateInode success, fsId = " << fsId << ", length = " << length
            << ", uid = " << uid << ", gid = " << gid << ", mode = " << mode
            << ", type =" << FsFileType_Name(type) << ", symlink = " << symlink
            << ", rdev = " << rdev
            << " ," << inode.ShortDebugString();

    return MetaStatusCode::OK;
}

MetaStatusCode InodeManager::CreateRootInode(uint32_t fsId, uint32_t uid,
                                             uint32_t gid, uint32_t mode) {
    VLOG(1) << "CreateRootInode, fsId = " << fsId << ", uid = " << uid
            << ", gid = " << gid << ", mode = " << mode;

    // 1. generate inode
    Inode inode;
    uint64_t length = 0;
    GenerateInodeInternal(ROOTINODEID, fsId, length, uid, gid, mode,
                          FsFileType::TYPE_DIRECTORY, 0, &inode);
    // 2. insert inode
    MetaStatusCode ret = inodeStorage_->Insert(inode);
    if (ret != MetaStatusCode::OK) {
        LOG(ERROR) << "CreateRootInode fail, fsId = " << fsId
                   << ", uid = " << uid << ", gid = " << gid
                   << ", mode = " << mode
                   << ", ret = " << MetaStatusCode_Name(ret);
        return ret;
    }

    VLOG(1) << "CreateRootInode success, inode: " << inode.ShortDebugString();
    return MetaStatusCode::OK;
}

void InodeManager::GenerateInodeInternal(uint64_t inodeId, uint32_t fsId,
                                         uint64_t length, uint32_t uid,
                                         uint32_t gid, uint32_t mode,
                                         FsFileType type, uint64_t rdev,
                                         Inode *inode) {
    inode->set_inodeid(inodeId);
    inode->set_fsid(fsId);
    inode->set_length(length);
    inode->set_uid(uid);
    inode->set_gid(gid);
    inode->set_mode(mode);
    inode->set_type(type);
    inode->set_rdev(rdev);

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    inode->set_mtime(now.tv_sec);
    inode->set_mtime_ns(now.tv_nsec);
    inode->set_atime(now.tv_sec);
    inode->set_atime_ns(now.tv_nsec);
    inode->set_ctime(now.tv_sec);
    inode->set_ctime_ns(now.tv_nsec);

    inode->set_openflag(false);
    if (FsFileType::TYPE_DIRECTORY == type) {
        inode->set_nlink(2);
    } else {
        inode->set_nlink(1);
    }
    return;
}

MetaStatusCode InodeManager::GetInode(uint32_t fsId, uint64_t inodeId,
                                      Inode *inode) {
    VLOG(1) << "GetInode, fsId = " << fsId << ", inodeId = " << inodeId;
    NameLockGuard lg(inodeLock_, GetInodeLockName(fsId, inodeId));
    MetaStatusCode ret = inodeStorage_->GetCopy(InodeKey(fsId, inodeId), inode);
    if (ret != MetaStatusCode::OK) {
        LOG(ERROR) << "GetInode fail, fsId = " << fsId
                   << ", inodeId = " << inodeId
                   << ", ret = " << MetaStatusCode_Name(ret);
        return ret;
    }

    VLOG(1) << "GetInode success, fsId = " << fsId << ", inodeId = " << inodeId
            << ", " << inode->ShortDebugString();

    return MetaStatusCode::OK;
}

MetaStatusCode InodeManager::DeleteInode(uint32_t fsId, uint64_t inodeId) {
    VLOG(1) << "DeleteInode, fsId = " << fsId << ", inodeId = " << inodeId;
    NameLockGuard lg(inodeLock_, GetInodeLockName(fsId, inodeId));
    MetaStatusCode ret = inodeStorage_->Delete(InodeKey(fsId, inodeId));
    if (ret != MetaStatusCode::OK) {
        LOG(ERROR) << "DeleteInode fail, fsId = " << fsId
                   << ", inodeId = " << inodeId
                   << ", ret = " << MetaStatusCode_Name(ret);
        return ret;
    }

    VLOG(1) << "DeleteInode success, fsId = " << fsId
            << ", inodeId = " << inodeId;
    return MetaStatusCode::OK;
}

MetaStatusCode InodeManager::UpdateInode(const UpdateInodeRequest &request) {
    VLOG(1) << "UpdateInode, " << request.ShortDebugString();
    NameLockGuard lg(inodeLock_, GetInodeLockName(
            request.fsid(), request.inodeid()));

    std::shared_ptr<Inode> old;
    MetaStatusCode ret = inodeStorage_->Get(
        InodeKey(request.fsid(), request.inodeid()), &old);
    if (ret != MetaStatusCode::OK) {
        LOG(ERROR) << "GetInode fail, " << request.ShortDebugString()
                   << ", ret: " << MetaStatusCode_Name(ret);
        return ret;
    }

    bool needAddTrash = false;

#define UPDATE_INODE(param)                  \
    if (request.has_##param()) {            \
        old->set_##param(request.param()); \
    }

    UPDATE_INODE(length)
    UPDATE_INODE(ctime)
    UPDATE_INODE(ctime_ns)
    UPDATE_INODE(mtime)
    UPDATE_INODE(mtime_ns)
    UPDATE_INODE(atime)
    UPDATE_INODE(atime_ns)
    UPDATE_INODE(uid)
    UPDATE_INODE(gid)
    UPDATE_INODE(mode)

    if (request.has_nlink()) {
        if (old->nlink() != 0 && request.nlink() == 0) {
            uint32_t now = TimeUtility::GetTimeofDaySec();
            old->set_dtime(now);
            needAddTrash = true;
        }
        old->set_nlink(request.nlink());
    }

    if (request.has_volumeextentlist()) {
        VLOG(1) << "update inode has extent";
        old->mutable_volumeextentlist()->CopyFrom(request.volumeextentlist());
    }

    if (needAddTrash) {
        trash_->Add(old->fsid(), old->inodeid(), old->dtime());
    }

    VLOG(1) << "UpdateInode success, " << request.ShortDebugString();
    return MetaStatusCode::OK;
}

void MergeToS3ChunkInfoList(const S3ChunkInfoList &listToAdd,
    S3ChunkInfoList *listToMerge) {
    for (int i = 0; i < listToAdd.s3chunks_size(); i++) {
        auto &s3chunkInfo = listToAdd.s3chunks(i);
        auto info = listToMerge->add_s3chunks();
        info->CopyFrom(s3chunkInfo);
    }
}

void RemoveFromS3ChunkInfoList(const S3ChunkInfoList &listToRemove,
    S3ChunkInfoList *listToMerge) {
    S3ChunkInfoList newList;
    for (int i = 0; i < listToMerge->s3chunks_size(); i++) {
        auto &s3chunkInfo = listToMerge->s3chunks(i);

        auto s3Chunks = listToRemove.s3chunks();
        auto s3chunkIt =
            std::find_if(s3Chunks.begin(), s3Chunks.end(),
                         [&](const S3ChunkInfo& c) {
                             return MessageDifferencer::Equals(c, s3chunkInfo);
                         });
        if (s3chunkIt == s3Chunks.end()) {
            auto info = newList.add_s3chunks();
            info->CopyFrom(s3chunkInfo);
        }
    }
    listToMerge->Swap(&newList);
}

int ProcessRequestFromS3Compact(
    const google::protobuf::Map<uint64_t, S3ChunkInfoList>& s3ChunkInfoAdd,
    const google::protobuf::Map<uint64_t, S3ChunkInfoList>& s3ChunkInfoRemove,
    Inode* inode) {
    VLOG(9) << "s3compact: request from s3compaction, inodeId:"
            << inode->inodeid();
    auto checkWithLog = [](bool cond) {
        if (!cond)
            LOG(WARNING) << "s3compact: bad GetOrModifyS3ChunkInfo request";
        return cond;
    };
    if (!checkWithLog(s3ChunkInfoAdd.size() == s3ChunkInfoRemove.size()))
        return -1;
    for (const auto& item : s3ChunkInfoAdd) {
        const auto& chunkIndex = item.first;
        const auto& toAddList = item.second;
        if (!checkWithLog(toAddList.s3chunks_size() == 1)) return -1;
        if (!checkWithLog(s3ChunkInfoRemove.find(chunkIndex) !=
                          s3ChunkInfoRemove.end()))
            return -1;
        const auto& toRemoveList = s3ChunkInfoRemove.at(chunkIndex);
        const auto& origItem = inode->s3chunkinfomap().find(chunkIndex);
        if (!checkWithLog(origItem != inode->s3chunkinfomap().end())) return -1;
        // delete and insert
        const auto& compactChunkId = toAddList.s3chunks(0).chunkid();
        S3ChunkInfoList newList;
        const auto& origList = origItem->second;
        bool inserted = false;
        const auto& toRemoveS3chunks = toRemoveList.s3chunks();
        for (int i = 0; i < origList.s3chunks_size(); i++) {
            const auto& origS3chunk = origList.s3chunks(i);
            if (inserted) {
                // fastpath, all left s3chunks are needed
                *newList.add_s3chunks() = origS3chunk;
                continue;
            }
            auto s3chunkIt =
                std::find_if(toRemoveS3chunks.begin(), toRemoveS3chunks.end(),
                             [origS3chunk](const S3ChunkInfo& toRemoveS3chunk) {
                                 return MessageDifferencer::Equals(
                                     toRemoveS3chunk, origS3chunk);
                             });
            if (s3chunkIt == toRemoveS3chunks.end()) {
                if (origS3chunk.chunkid() > compactChunkId) {
                    *newList.add_s3chunks() = toAddList.s3chunks(0);
                    VLOG(9) << "s3compact: add chunkid "
                            << toAddList.s3chunks(0).chunkid();
                    inserted = true;
                }
                *newList.add_s3chunks() = origS3chunk;
                VLOG(9) << "s3compact: add chunkid " << origS3chunk.chunkid();
            }
        }
        if (!inserted) {
            *newList.add_s3chunks() = toAddList.s3chunks(0);
            VLOG(9) << "s3compact: add chunkid "
                    << toAddList.s3chunks(0).chunkid();
        }
        inode->mutable_s3chunkinfomap()->at(chunkIndex).Swap(&newList);
    }
    return 0;
}

MetaStatusCode InodeManager::GetOrModifyS3ChunkInfo(
    uint32_t fsId, uint64_t inodeId,
    const google::protobuf::Map<uint64_t, S3ChunkInfoList>& s3ChunkInfoAdd,
    const google::protobuf::Map<uint64_t, S3ChunkInfoList>& s3ChunkInfoRemove,
    bool returnS3ChunkInfoMap,
    google::protobuf::Map<uint64_t, S3ChunkInfoList>* out,
    bool fromS3Compaction) {
    VLOG(1) << "GetOrModifyS3ChunkInfo, fsId: " << fsId
            << ", inodeId: " << inodeId;

    NameLockGuard lg(inodeLock_, GetInodeLockName(
            fsId, inodeId));
    std::shared_ptr<Inode> old;
    MetaStatusCode ret = inodeStorage_->Get(
        InodeKey(fsId, inodeId), &old);
    if (ret != MetaStatusCode::OK) {
        LOG(ERROR) << "GetInode fail, fsId: " << fsId
                   << ", inodeId: " << inodeId
                   << ", ret: " << MetaStatusCode_Name(ret);
        return ret;
    }
    if (!s3ChunkInfoAdd.empty() || !s3ChunkInfoRemove.empty()) {
        bool duplicated = false;
        // judge if duplicated add or not
        if (!s3ChunkInfoAdd.empty()) {
            auto ix = s3ChunkInfoAdd.begin();
            auto it = old->mutable_s3chunkinfomap()->find(
                ix->first);
            if (it != old->mutable_s3chunkinfomap()->end()) {
                auto &s3chunkInfo = ix->second.s3chunks(0);
                auto s3Chunks = it->second.mutable_s3chunks();
                auto s3chunkIt =
                    std::find_if(s3Chunks->begin(), s3Chunks->end(),
                         [&](const S3ChunkInfo& c) {
                             return MessageDifferencer::Equals(c, s3chunkInfo);
                         });
                if (s3chunkIt != s3Chunks->end()) {
                    // duplicated
                    duplicated = true;
                }
            }
        }
        if (!duplicated) {
            if (fromS3Compaction) {
                int r = ProcessRequestFromS3Compact(s3ChunkInfoAdd,
                    s3ChunkInfoRemove, old.get());
                if (r == -1) {
                    return MetaStatusCode::PARAM_ERROR;
                }
            } else {
                for (auto &item : s3ChunkInfoAdd) {
                    auto it = old->mutable_s3chunkinfomap()->find(item.first);
                    if (it != old->mutable_s3chunkinfomap()->end()) {
                        MergeToS3ChunkInfoList(item.second, &(it->second));
                    } else {
                        old->mutable_s3chunkinfomap()->insert(
                            {item.first, item.second});
                    }
                }

                for (auto &item : s3ChunkInfoRemove) {
                    auto it = old->mutable_s3chunkinfomap()->find(item.first);
                    if (it != old->mutable_s3chunkinfomap()->end()) {
                        RemoveFromS3ChunkInfoList(item.second, &(it->second));
                        if (0 == it->second.s3chunks_size()) {
                            old->mutable_s3chunkinfomap()->erase(it);
                        }
                    }
                }
            }
        }
    }
    if (returnS3ChunkInfoMap) {
        *out = old->s3chunkinfomap();
    }

    VLOG(1) << "GetOrModifyS3ChunkInfo success, fsId: " << fsId
            << ", inodeId: " << inodeId;

    return MetaStatusCode::OK;
}

MetaStatusCode InodeManager::UpdateInodeWhenCreateOrRemoveSubNode(
    uint32_t fsId, uint64_t inodeId, bool isCreate) {
    VLOG(1) << "UpdateInodeWhenCreateOrRemoveSubNode, fsId = " << fsId
            << ", inodeId = " << inodeId
            << ", isCreate = " << isCreate;
    NameLockGuard lg(inodeLock_, GetInodeLockName(fsId, inodeId));

    std::shared_ptr<Inode> inode;
    MetaStatusCode ret = inodeStorage_->Get(
        InodeKey(fsId, inodeId), &inode);
    if (ret != MetaStatusCode::OK) {
        LOG(ERROR) << "GetInode fail, " << inode->ShortDebugString()
                   << ", ret = " << MetaStatusCode_Name(ret);
        return ret;
    }
    uint32_t oldNlink = inode->nlink();
    if (oldNlink == 0) {
        // already be deleted
        return MetaStatusCode::OK;
    }
    if (isCreate) {
        inode->set_nlink(++oldNlink);
    } else {
        inode->set_nlink(--oldNlink);
    }

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    inode->set_ctime(now.tv_sec);
    inode->set_ctime_ns(now.tv_nsec);
    inode->set_mtime(now.tv_sec);
    inode->set_mtime_ns(now.tv_nsec);

    VLOG(1) << "UpdateInodeWhenCreateOrRemoveSubNode success, "
            << inode->ShortDebugString();
    return MetaStatusCode::OK;
}

MetaStatusCode InodeManager::InsertInode(const Inode &inode) {
    VLOG(1) << "InsertInode, " << inode.ShortDebugString();

    // 2. insert inode
    MetaStatusCode ret = inodeStorage_->Insert(inode);
    if (ret != MetaStatusCode::OK) {
        LOG(ERROR) << "InsertInode fail, " << inode.ShortDebugString()
                   << ", ret = " << MetaStatusCode_Name(ret);
        return ret;
    }

    if (inode.nlink() == 0) {
        trash_->Add(inode.fsid(), inode.inodeid(), inode.dtime());
    }

    return MetaStatusCode::OK;
}

void InodeManager::GetInodeIdList(std::list<uint64_t>* inodeIdList) {
    inodeStorage_->GetInodeIdList(inodeIdList);
}

}  // namespace metaserver
}  // namespace curvefs
