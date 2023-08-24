#include "channel_manager.h"

using namespace server::common;

ChannelManager::ChannelManager() { }

SharedPtrChannel ChannelManager::GetChannel(const std::string& service_name,
                                            GroupStrategy group_strategy,
                                            const std::string& lb,
                                            uint32_t request_code,
                                            brpc::ChannelOptions* options) {
    /**
     * service_name(for example: "mc://brpc_name_agent")
     * real_service_name(for example: "mc://brpc_name_agent:0")
     * service_name_with_lb(for example: "mc://brpc_name_agent:0:rr")
    **/

    std::string real_service_name    = service_name + ":" + std::to_string(group_strategy);
    std::string service_name_with_lb = real_service_name + ":" + lb;
    {
        butil::DoublyBufferedData<ServerChannels>::ScopedPtr map_ptr;
        if (_server_channels.Read(&map_ptr) != 0) {
            LOG(ERROR) << "[!] Fail to read m_instancesByRegion.";
            return nullptr;
        }

        const ServerChannels& m = *(map_ptr.get());
        if (m.count(service_name_with_lb)) {
            return m.at(service_name_with_lb);
        }
    }

    SharedPtrChannel channel(new brpc::Channel);
    {
        BAIDU_SCOPED_LOCK(_mutex);
        if (channel->Init(real_service_name.c_str(), lb.c_str(), options) != 0) {
            LOG(ERROR) << "[!] Failed to new Channel, service_name_with_lb:" << service_name_with_lb;
            return nullptr;
        }
    }

    auto modify_fptr = [&service_name_with_lb, &channel](ServerChannels& map) -> int {
        map.insert(std::make_pair(service_name_with_lb, channel));
        return 1;
    };
    _server_channels.Modify(modify_fptr);
    return channel;
}