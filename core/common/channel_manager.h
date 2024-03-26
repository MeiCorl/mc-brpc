#pragma once

#include <unordered_map>
#include <brpc/channel.h>
#include <bthread/mutex.h>
#include "butil/memory/singleton.h"
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
    ServerChannels _server_channels;

public:
    static ChannelManager* GetInstance() {
        return Singleton<ChannelManager>::get();
    }

    SharedPtrChannel GetChannel(const std::string& service_name,
                                GroupStrategy group_strategy  = GroupStrategy::STRATEGY_NORMAL,
                                const std::string& lb         = "rr",
                                brpc::ChannelOptions* options = nullptr);
};

} // namespace common
} // namespace server