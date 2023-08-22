#pragma once

#include <memory>
#include <unordered_map>
#include <brpc/channel.h>
#include <bthread/bthread.h>
#include <butil/containers/doubly_buffered_data.h>
#include "core/utils/singleton.h"
#include "core/config/server_config.h"
#include "agent_service.pb.h"

namespace server {
namespace common {

using SharedPtrChannel = std::shared_ptr<brpc::Channel>;
using namespace server::config;

class ChannelManager {
    typedef std::unordered_map<std::string, SharedPtrChannel> ChannelsMap;
    typedef std::unordered_map<std::string, ChannelsMap> ServerChannelsMap;

private:
    bool _name_agent_inited;
    brpc::Channel _name_agent_channel;
    std::shared_ptr<name_agent::AgentService_Stub> _name_agent_stub;

    butil::DoublyBufferedData<ServerChannelsMap> _server_channels;

public:
    ChannelManager();
    SharedPtrChannel GetChannel(const std::string& service_name,
                                GroupStrategy group_strategy  = GroupStrategy::STRATEGY_NORMAL,
                                LbStrategy lb_strategy        = LbStrategy::rr,
                                uint32_t request_code         = 0,
                                uint32_t group_request_code   = 0,
                                brpc::ChannelOptions* options = nullptr);
};

using SingletonChannel = server::utils::Singleton<ChannelManager>;

} // namespace common
} // namespace server