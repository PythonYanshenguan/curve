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
#include "curvefs/src/client/dentry_cache_manager.h"

#include <string>
#include <list>
#include <vector>
#include <utility>
#include <unordered_map>

using ::curvefs::metaserver::MetaStatusCode_Name;

namespace curvefs {
namespace client {

using curve::common::WriteLockGuard;
using NameLockGuard = ::curve::common::GenericNameLockGuard<Mutex>;

void DentryCacheManagerImpl::InsertOrReplaceCache(const Dentry &dentry) {
    std::string key = GetDentryCacheKey(dentry.parentinodeid(), dentry.name());
    NameLockGuard lock(nameLock_, key);
    dCache_->Put(key, dentry);
}

void DentryCacheManagerImpl::DeleteCache(uint64_t parentId,
                                         const std::string &name) {
    std::string key = GetDentryCacheKey(parentId, name);
    NameLockGuard lock(nameLock_, key);
    dCache_->Remove(key);
}

CURVEFS_ERROR DentryCacheManagerImpl::GetDentry(uint64_t parent,
                                                const std::string &name,
                                                Dentry *out) {
    std::string key = GetDentryCacheKey(parent, name);
    NameLockGuard lock(nameLock_, key);
    bool ok = dCache_->Get(key, out);
    if (ok) {
        return CURVEFS_ERROR::OK;
    }

    MetaStatusCode ret = metaClient_->GetDentry(fsId_, parent, name, out);
    if (ret != MetaStatusCode::OK) {
        LOG_IF(ERROR, ret != MetaStatusCode::NOT_FOUND)
            << "metaClient_ GetDentry failed, MetaStatusCode = " << ret
            << ", MetaStatusCode_Name = " << MetaStatusCode_Name(ret)
            << ", parent = " << parent << ", name = " << name;
        return MetaStatusCodeToCurvefsErrCode(ret);
    }

    dCache_->Put(key, *out);
    return CURVEFS_ERROR::OK;
}

CURVEFS_ERROR DentryCacheManagerImpl::CreateDentry(const Dentry &dentry) {
    std::string key = GetDentryCacheKey(dentry.parentinodeid(), dentry.name());
    NameLockGuard lock(nameLock_, key);
    MetaStatusCode ret = metaClient_->CreateDentry(dentry);
    if (ret != MetaStatusCode::OK) {
        LOG(ERROR) << "metaClient_ CreateDentry failed, MetaStatusCode = "
                   << ret
                   << ", MetaStatusCode_Name = " << MetaStatusCode_Name(ret)
                   << ", parent = " << dentry.parentinodeid()
                   << ", name = " << dentry.name();
        return MetaStatusCodeToCurvefsErrCode(ret);
    }

    dCache_->Put(key, dentry);
    return CURVEFS_ERROR::OK;
}

CURVEFS_ERROR DentryCacheManagerImpl::DeleteDentry(uint64_t parent,
                                                   const std::string &name) {
    std::string key = GetDentryCacheKey(parent, name);
    NameLockGuard lock(nameLock_, key);
    dCache_->Remove(key);

    MetaStatusCode ret = metaClient_->DeleteDentry(fsId_, parent, name);
    if (ret != MetaStatusCode::OK && ret != MetaStatusCode::NOT_FOUND) {
        LOG(ERROR) << "metaClient_ DeleteInode failed, MetaStatusCode = " << ret
                   << ", MetaStatusCode_Name = " << MetaStatusCode_Name(ret)
                   << ", parent = " << parent << ", name = " << name;
        return MetaStatusCodeToCurvefsErrCode(ret);
    }
    return CURVEFS_ERROR::OK;
}

CURVEFS_ERROR DentryCacheManagerImpl::ListDentry(uint64_t parent,
                                                 std::list<Dentry> *dentryList,
                                                 uint32_t limit) {
    bool perceed = true;
    MetaStatusCode ret = MetaStatusCode::OK;
    dentryList->clear();
    std::string last = "";
    do {
        std::list<Dentry> part;
        ret = metaClient_->ListDentry(fsId_, parent, last, limit, &part);
        VLOG(6) << "ListDentry fsId = " << fsId_ << ", parent = " << parent
                << ", last = " << last << ", count = " << limit
                << ", ret = " << ret << ", part.size() = " << part.size();
        if (ret != MetaStatusCode::OK) {
            if (MetaStatusCode::NOT_FOUND == ret) {
                return CURVEFS_ERROR::OK;
            }
            LOG(ERROR) << "metaClient_ ListDentry failed, MetaStatusCode = "
                       << ret
                       << ", MetaStatusCode_Name = " << MetaStatusCode_Name(ret)
                       << ", parent = " << parent << ", last = " << last
                       << ", count = " << limit;
            return MetaStatusCodeToCurvefsErrCode(ret);
        }
        if (part.size() < limit) {
            perceed = false;
        }
        if (!part.empty()) {
            last = part.back().name();
            dentryList->splice(dentryList->end(), part);
        }
    } while (perceed);

    return CURVEFS_ERROR::OK;
}

}  // namespace client
}  // namespace curvefs
