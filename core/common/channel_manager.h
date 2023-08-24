#pragma once

#include <unordered_map>
#include <brpc/channel.h>
#include <bthread/mutex.h>
#include <butil/containers/doubly_buffered_data.h>
#include "core/utils/singleton.h"
#include "core/config/server_config.h"

namespace server {
namespace common {

#define SVR_NAME_PREFIX "mc://"
#define MakeServiceName(orginal_service_name) SVR_NAME_PREFIX + orginal_service_name

using SharedPtrChannel = std::shared_ptr<brpc::Channel>;
using namespace server::config;

class ChannelManager {
    typedef std::unordered_map<std::string, SharedPtrChannel> ServerChannels;

private:
    bthread::Mutex _mutex;
    butil::DoublyBufferedData<ServerChannels> _server_channels;

public:
    ChannelManager();
    SharedPtrChannel GetChannel(const std::string& service_name,
                                GroupStrategy group_strategy  = GroupStrategy::STRATEGY_NORMAL,
                                const std::string& lb         = "rr",
                                uint32_t request_code         = 0,
                                brpc::ChannelOptions* options = nullptr);
};

using SingletonChannel = server::utils::Singleton<ChannelManager>;

} // namespace common
} // namespace server