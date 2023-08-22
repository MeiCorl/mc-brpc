#include "channel_manager.h"

using namespace server::common;

DEFINE_string(name_agent_url, "unix:/var/brpc_name_agent.sock", "addr of brpc_name_agent");

ChannelManager::ChannelManager() {
    brpc::ChannelOptions options;
    options.connection_type    = "single";
    options.connect_timeout_ms = 1000;
    options.timeout_ms         = 1500;
    options.max_retry          = 2;
    if (_name_agent_channel.Init(FLAGS_name_agent_url.c_str(), &options) != 0) {
        LOG(FATAL) << "[!] Fail to init channel to " << FLAGS_name_agent_url;
        _name_agent_inited = false;
        return;
    }

    _name_agent_inited = true;
    _name_agent_stub.reset(new name_agent::AgentService_Stub(&_name_agent_channel));
}

SharedPtrChannel ChannelManager::GetChannel(const std::string& service_name,
                                            GroupStrategy group_strategy,
                                            LbStrategy lb_strategy,
                                            uint32_t request_code,
                                            uint32_t group_request_code,
                                            brpc::ChannelOptions* options) {
    if (!_name_agent_inited) {
        return nullptr;
    }

    name_agent::GetUpstreamInstanceReq req;
    name_agent::GetUpstreamInstanceRes res;
    req.set_seq_id(butil::gettimeofday_ms());
    req.set_service_name(service_name);
    req.set_group_strategy(group_strategy);
    req.set_lb_strategy(lb_strategy);
    req.set_request_code(request_code);
    req.set_group_request_code(group_request_code);

    brpc::Controller cntl;
    _name_agent_stub->GetUpstreamInstance(&cntl, &req, &res, nullptr);
    if (!cntl.Failed()) {
        LOG(DEBUG) << "[+] get endpoint " << res.endpoint() << " latency=" << cntl.latency_us()
                   << "us";
        const std::string& endponit = res.endpoint();
        if (endponit.empty()) {
            LOG(ERROR) << "[!] Fail to select server of " << service_name;
            return nullptr;
        }

        {
            butil::DoublyBufferedData<ServerChannelsMap>::ScopedPtr map_ptr;
            if (_server_channels.Read(&map_ptr) != 0) {
                LOG(ERROR) << "[!] Fail to read m_instancesByRegion.";
                return nullptr;
            }
            const ServerChannelsMap& m = *(map_ptr.get());
            if (m.count(service_name) && m.at(service_name).count(endponit)) {
                return m.at(service_name).at(endponit);
            }
        }

        SharedPtrChannel channel(new brpc::Channel);
        if (channel->Init(endponit.c_str(), options) != 0) {
            LOG(ERROR) << "[!] Fail to init channel to " << service_name;
            return nullptr;
        }

        LOG(INFO) << "[+] Init channel to service " << service_name << "(" << endponit << ")";

        auto modify_fptr = [&service_name, &endponit, &channel](ServerChannelsMap& map) -> int {
            map[service_name][endponit] = channel;
            return 1;
        };
        _server_channels.Modify(modify_fptr);

        return channel;
    } else {
        LOG(ERROR) << "[!] GetUpstreamInstance Error: " << cntl.ErrorText();
        return nullptr;
    }
}