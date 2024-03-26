#include "channel_manager.h"

using namespace server::common;

SharedPtrChannel ChannelManager::GetChannel(const std::string& service_name,
                                            GroupStrategy group_strategy,
                                            const std::string& lb,
                                            brpc::ChannelOptions* options) {
    /**
     * service_name(for example: "mc://brpc_name_agent")
     * real_service_name(for example: "mc://brpc_name_agent:0")
     * service_name_with_lb(for example: "mc://brpc_name_agent:0:rr")
    **/

    std::string real_service_name    = service_name + ":" + std::to_string(group_strategy);
    std::string service_name_with_lb = real_service_name + ":" + lb;

    BAIDU_SCOPED_LOCK(_mutex);
    auto it = _server_channels.find(service_name_with_lb);
    if (it != _server_channels.end()) {
        return it->second;
    }

    SharedPtrChannel channel(new brpc::Channel);
    if (channel->Init(real_service_name.c_str(), lb.c_str(), options) != 0) {
        LOG(ERROR) << "[!] Failed to new Channel, service_name_with_lb:" << service_name_with_lb;
        return nullptr;
    }

    _server_channels[service_name_with_lb] = channel;
    return channel;
}