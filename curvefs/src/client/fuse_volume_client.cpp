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
 * Created Date: Thur May 27 2021
 * Author: xuchaojie
 */

#include <string>
#include <list>

#include <memory>

#include "curvefs/src/client/fuse_volume_client.h"

namespace curvefs {
namespace client {

CURVEFS_ERROR FuseVolumeClient::Init(const FuseClientOption &option) {
    volOpts_ = option.volumeOpt;

    CURVEFS_ERROR ret = FuseClient::Init(option);
    if (ret != CURVEFS_ERROR::OK) {
        return ret;
    }
    spaceBase_ = std::make_shared<SpaceBaseClient>();
    ret = spaceClient_->Init(option.spaceOpt, spaceBase_.get());
    if (ret != CURVEFS_ERROR::OK) {
        return ret;
    }

    ret = extManager_->Init(option.extentManagerOpt);
    if (ret != CURVEFS_ERROR::OK) {
        return ret;
    }
    ret = blockDeviceClient_->Init(option.bdevOpt);
    return ret;
}

void FuseVolumeClient::UnInit() {
    blockDeviceClient_->UnInit();
    FuseClient::UnInit();
}

CURVEFS_ERROR FuseVolumeClient::CreateFs(void *userdata, FsInfo *fsInfo) {
    struct MountOption *mOpts = (struct MountOption *)userdata;
    std::string volName = (mOpts->volume == nullptr) ? "" : mOpts->volume;
    std::string fsName = (mOpts->fsName == nullptr) ? "" : mOpts->fsName;
    std::string user = (mOpts->user == nullptr) ? "" : mOpts->user;

    CURVEFS_ERROR ret = CURVEFS_ERROR::OK;
    BlockDeviceStat stat;
    ret = blockDeviceClient_->Stat(volName, user, &stat);
    if (ret != CURVEFS_ERROR::OK) {
        LOG(ERROR) << "Stat volume failed, ret = " << ret
                   << ", volName = " << volName << ", user = " << user;
        return ret;
    }

    Volume vol;
    vol.set_volumesize(stat.length);
    vol.set_blocksize(volOpts_.volBlockSize);
    vol.set_volumename(volName);
    vol.set_user(user);

    FSStatusCode ret2 = mdsClient_->CreateFs(fsName, volOpts_.fsBlockSize, vol);
    if (ret2 != FSStatusCode::OK) {
        return CURVEFS_ERROR::INTERNAL;
    }
    return CURVEFS_ERROR::OK;
}

CURVEFS_ERROR FuseVolumeClient::FuseOpInit(
    void *userdata, struct fuse_conn_info *conn) {
    struct MountOption *mOpts = (struct MountOption *) userdata;
    std::string volName = (mOpts->volume == nullptr) ? "" : mOpts->volume;
    std::string user = (mOpts->user == nullptr) ? "" : mOpts->user;
    CURVEFS_ERROR ret = blockDeviceClient_->Open(volName, user);
    if (ret != CURVEFS_ERROR::OK) {
        LOG(ERROR) << "BlockDeviceClientImpl open failed, ret = " << ret
        << ", volName = " << volName
        << ", user = " << user;
        return ret;
    }
    return FuseClient::FuseOpInit(userdata, conn);
}

void FuseVolumeClient::FuseOpDestroy(void *userdata) {
    FuseClient::FuseOpDestroy(userdata);
    CURVEFS_ERROR ret = blockDeviceClient_->Close();
    if (ret != CURVEFS_ERROR::OK) {
        LOG(ERROR) << "BlockDeviceClientImpl close failed, ret = " << ret;
        return;
    }
    return;
}

CURVEFS_ERROR FuseVolumeClient::FuseOpWrite(fuse_req_t req, fuse_ino_t ino,
                                            const char *buf, size_t size,
                                            off_t off,
                                            struct fuse_file_info *fi,
                                            size_t *wSize) {
    // check align
    if (fi->flags & O_DIRECT) {
        if (!(is_aligned(off, DirectIOAlignemnt) &&
              is_aligned(size, DirectIOAlignemnt)))
            return CURVEFS_ERROR::INVALIDPARAM;
    }

    std::shared_ptr<InodeWrapper> inodeWrapper;
    CURVEFS_ERROR ret = inodeManager_->GetInode(ino, inodeWrapper);
    if (ret != CURVEFS_ERROR::OK) {
        LOG(ERROR) << "inodeManager get inode fail, ret = " << ret
                   << ", inodeid = " << ino;
        return ret;
    }

    ::curve::common::UniqueLock lgGuard = inodeWrapper->GetUniqueLock();
    Inode inode = inodeWrapper->GetInodeUnlocked();

    std::list<ExtentAllocInfo> toAllocExtents;
    // get the extent need to be allocate
    ret = extManager_->GetToAllocExtents(inode.volumeextentlist(), off, size,
                                         &toAllocExtents);
    if (ret != CURVEFS_ERROR::OK) {
        LOG(ERROR) << "GetToAllocExtents fail, ret = " << ret;
        return ret;
    }
    if (toAllocExtents.size() != 0) {
        AllocateType type = AllocateType::NONE;
        if (inode.length() >= volOpts_.bigFileSize ||
            size >= volOpts_.bigFileSize) {
            type = AllocateType::BIG;
        } else {
            type = AllocateType::SMALL;
        }
        std::list<Extent> allocatedExtents;
        // to alloc extents
        ret = spaceClient_->AllocExtents(fsInfo_->fsid(), toAllocExtents, type,
                                         &allocatedExtents);
        if (ret != CURVEFS_ERROR::OK) {
            LOG(ERROR) << "metaClient alloc extents fail, ret = " << ret;
            return ret;
        }
        // merge the allocated extent to inode
        ret = extManager_->MergeAllocedExtents(
            toAllocExtents, allocatedExtents, inode.mutable_volumeextentlist());
        if (ret != CURVEFS_ERROR::OK) {
            LOG(ERROR) << "toAllocExtents and allocatedExtents not match, "
                       << "ret = " << ret;
            CURVEFS_ERROR ret2 =
                spaceClient_->DeAllocExtents(fsInfo_->fsid(), allocatedExtents);
            if (ret2 != CURVEFS_ERROR::OK) {
                LOG(ERROR) << "DeAllocExtents fail, ret = " << ret;
            }
            return ret;
        }
    }

    // divide the extents which is write or not
    std::list<PExtent> pExtents;
    ret = extManager_->DivideExtents(inode.volumeextentlist(), off, size,
                                     &pExtents);
    if (ret != CURVEFS_ERROR::OK) {
        LOG(ERROR) << "DivideExtents fail, ret = " << ret;
        return ret;
    }

    // write the physical extents
    uint64_t writeLen = 0;
    for (const auto &ext : pExtents) {
        ret = blockDeviceClient_->Write(buf + writeLen, ext.pOffset, ext.len);
        writeLen += ext.len;
        if (ret != CURVEFS_ERROR::OK) {
            LOG(ERROR) << "block device write fail, ret = " << ret;
            return ret;
        }
    }

    // make the unwritten flag in the inode.
    ret = extManager_->MarkExtentsWritten(off, size,
                                          inode.mutable_volumeextentlist());
    if (ret != CURVEFS_ERROR::OK) {
        LOG(ERROR) << "MarkExtentsWritten fail, ret =  " << ret;
        return ret;
    }
    *wSize = size;
    // update file len
    if (inode.length() < off + *wSize) {
        inode.set_length(off + *wSize);
    }

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    inode.set_mtime(now.tv_sec);
    inode.set_mtime_ns(now.tv_nsec);
    inode.set_ctime(now.tv_sec);
    inode.set_ctime_ns(now.tv_nsec);

    inodeWrapper->SwapInode(&inode);
    inodeManager_->ShipToFlush(inodeWrapper);

    if (fi->flags & O_DIRECT || fi->flags & O_SYNC || fi->flags & O_DSYNC) {
        // Todo: do some cache flush later
    }
    return ret;
}

CURVEFS_ERROR FuseVolumeClient::FuseOpRead(fuse_req_t req, fuse_ino_t ino,
                                           size_t size, off_t off,
                                           struct fuse_file_info *fi,
                                           char *buffer, size_t *rSize) {
    // check align
    if (fi->flags & O_DIRECT) {
        if (!(is_aligned(off, DirectIOAlignemnt) &&
              is_aligned(size, DirectIOAlignemnt)))
            return CURVEFS_ERROR::INVALIDPARAM;
    }

    std::shared_ptr<InodeWrapper> inodeWrapper;
    CURVEFS_ERROR ret = inodeManager_->GetInode(ino, inodeWrapper);
    if (ret != CURVEFS_ERROR::OK) {
        LOG(ERROR) << "inodeManager get inode fail, ret = " << ret
                   << ", inodeid = " << ino;
        return ret;
    }

    ::curve::common::UniqueLock lgGuard = inodeWrapper->GetUniqueLock();
    Inode inode = inodeWrapper->GetInodeUnlocked();

    size_t len = 0;
    if (inode.length() <= off) {
        *rSize = 0;
        return CURVEFS_ERROR::OK;
    } else if (inode.length() < off + size) {
        len = inode.length() - off;
    } else {
        len = size;
    }
    std::list<PExtent> pExtents;
    ret = extManager_->DivideExtents(inode.volumeextentlist(), off, len,
                                     &pExtents);
    if (ret != CURVEFS_ERROR::OK) {
        LOG(ERROR) << "DivideExtents fail, ret = " << ret;
        return ret;
    }
    uint64_t readOff = 0;
    for (const auto &ext : pExtents) {
        if (!ext.UnWritten) {
            ret = blockDeviceClient_->Read(buffer + readOff, ext.pOffset,
                                           ext.len);
            if (ret != CURVEFS_ERROR::OK) {
                LOG(ERROR) << "block device read fail, ret = " << ret;
                return ret;
            }
        }
        readOff += ext.len;
    }
    *rSize = len;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    inode.set_ctime(now.tv_sec);
    inode.set_ctime_ns(now.tv_nsec);
    inode.set_atime(now.tv_sec);
    inode.set_atime_ns(now.tv_nsec);

    inodeWrapper->SwapInode(&inode);
    inodeManager_->ShipToFlush(inodeWrapper);

    VLOG(6) << "read end, read size = " << *rSize;
    return ret;
}

CURVEFS_ERROR FuseVolumeClient::FuseOpCreate(fuse_req_t req, fuse_ino_t parent,
                                             const char *name, mode_t mode,
                                             struct fuse_file_info *fi,
                                             fuse_entry_param *e) {
    LOG(INFO) << "FuseOpCreate, parent: " << parent
              << ", name: " << name
              << ", mode: " << mode;
    CURVEFS_ERROR ret =
        MakeNode(req, parent, name, mode, FsFileType::TYPE_FILE, 0, e);
    if (ret != CURVEFS_ERROR::OK) {
        return ret;
    }
    return FuseOpOpen(req, e->ino, fi);
}

CURVEFS_ERROR FuseVolumeClient::FuseOpMkNod(fuse_req_t req, fuse_ino_t parent,
                                            const char *name, mode_t mode,
                                            dev_t rdev, fuse_entry_param *e) {
    LOG(INFO) << "FuseOpMkNod, parent: " << parent
              << ", name: " << name
              << ", mode: " << mode
              << ", rdev: " << rdev;
    return MakeNode(req, parent, name, mode, FsFileType::TYPE_FILE, rdev, e);
}

CURVEFS_ERROR FuseVolumeClient::FuseOpFsync(fuse_req_t req, fuse_ino_t ino,
                                            int datasync,
                                            struct fuse_file_info *fi) {
    LOG(INFO) << "FuseOpFsync, ino: " << ino << ", datasync: " << datasync;
    return CURVEFS_ERROR::NOTSUPPORT;
}

CURVEFS_ERROR FuseVolumeClient::Truncate(Inode *inode, uint64_t length) {
    // Todo: call volume truncate
    return CURVEFS_ERROR::OK;
}

void FuseVolumeClient::FlushData() {
    // TODO(xuchaojie) : flush volume data
}

}  // namespace client
}  // namespace curvefs
