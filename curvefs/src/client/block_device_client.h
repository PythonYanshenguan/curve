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

#ifndef CURVEFS_SRC_CLIENT_BLOCK_DEVICE_CLIENT_H_
#define CURVEFS_SRC_CLIENT_BLOCK_DEVICE_CLIENT_H_

#include <memory>
#include <utility>
#include <string>

#include "src/client/client_common.h"
#include "src/client/libcurve_file.h"
#include "curvefs/src/client/common/config.h"
#include "curvefs/src/client/error_code.h"

namespace curvefs {
namespace client {

using ::curve::client::FileClient;
using common::BlockDeviceClientOptions;

using Range = std::pair<off_t, off_t>;

enum class BlockDeviceStatus {
    CREATED,
    DELETING,
    CLONING,
    CLONE_META_INSTALLED,
    CLONED,
    BEING_CLONED
};

struct BlockDeviceStat {
    uint64_t length;
    BlockDeviceStatus status;
};

class BlockDeviceClient {
 public:
    BlockDeviceClient() {}
    virtual ~BlockDeviceClient() {}

    /**
     * @brief Initailize client
     * @param[in] options the options for client
     * @return error code (CURVEFS_ERROR:*)
     */
    virtual CURVEFS_ERROR Init(const BlockDeviceClientOptions& options) = 0;

    /**
     * @brief Uninitailize client
     */
    virtual void UnInit() = 0;

    /**
     * @brief Open file specify by filename and owner
     * @param[in] filename
     * @param[in] owner owner for filename
     * @return error code (CURVEFS_ERROR:*)
     */
    virtual CURVEFS_ERROR Open(const std::string& filename,
                               const std::string& owner) = 0;

    /**
     * @brief Close the fd which init by Open()
     * @return error code (CURVEFS_ERROR:*)
     */
    virtual CURVEFS_ERROR Close() = 0;

    /**
     * @brief Get file status
     * @param[in] filename
     * @param[in] owner owner for filename
     * @param[out] statInfo the struct for file status
     * @return error code (CURVEFS_ERROR:*)
     */
    virtual CURVEFS_ERROR Stat(const std::string& filename,
                               const std::string& owner,
                               BlockDeviceStat* statInfo) = 0;

    /**
     * @brief Read from fd which init by Open()
     * @param[in] buf read buffer
     * @param[in] offset read start offset
     * @param[in] length read length
     * @return error code (CURVEFS_ERROR:*)
     * @note the offset and length maybe not aligned
     */
    virtual CURVEFS_ERROR Read(char* buf, off_t offset, size_t length) = 0;

    /**
     * @brief Write to fd which init by Open()
     * @param[in] buf write buffer
     * @param[in] offset write start offset
     * @param[in] length write length
     * @return error code (CURVEFS_ERROR:*)
     * @note the offset and length maybe not aligned
     */
    virtual CURVEFS_ERROR Write(
        const char* buf, off_t offset, size_t length) = 0;
};

class BlockDeviceClientImpl : public BlockDeviceClient {
 public:
    BlockDeviceClientImpl();

    explicit BlockDeviceClientImpl(
        const std::shared_ptr<FileClient>& fileClient);

    ~BlockDeviceClientImpl() = default;

    CURVEFS_ERROR Init(const BlockDeviceClientOptions& options) override;

    void UnInit() override;

    CURVEFS_ERROR Open(const std::string& filename,
                       const std::string& owner) override;

    CURVEFS_ERROR Close() override;

    CURVEFS_ERROR Stat(const std::string& filename,
                       const std::string& owner,
                       BlockDeviceStat* statInfo) override;

    CURVEFS_ERROR Read(char* buf, off_t offset, size_t length) override;

    CURVEFS_ERROR Write(const char* buf, off_t offset, size_t length) override;

 private:
    CURVEFS_ERROR WritePadding(char* writeBuffer, off_t writeStart,
                               off_t writeEnd, off_t offset, size_t length);

    CURVEFS_ERROR AlignRead(char* buf, off_t offset, size_t length);

    CURVEFS_ERROR AlignWrite(const char* buf, off_t offset, size_t length);

    bool IsAligned(off_t offset, size_t length);

    off_t Align(off_t offset, size_t alignment);

    Range CalcAlignRange(off_t start, off_t end);

    bool ConvertFileStatus(int fileStatus, BlockDeviceStatus* bdStatus);

 private:
    int64_t fd_;

    std::string filename_;

    std::string owner_;

    std::shared_ptr<FileClient> fileClient_;
};

}  // namespace client
}  // namespace curvefs

#endif  // CURVEFS_SRC_CLIENT_BLOCK_DEVICE_CLIENT_H_
