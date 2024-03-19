#include "name_agent_client.h"
#include "common_callback.h"

using namespace server::common;

DEFINE_string(name_agent_url, "unix:/var/brpc_name_agent.sock", "addr of brpc_name_agent");

NameAgentClient* NameAgentClient::GetInstance() {
    return Singleton<NameAgentClient>::get();
}

NameAgentClient::NameAgentClient() {
    brpc::ChannelOptions options;
    options.connection_type = "single";
    options.connect_timeout_ms = 1000;
    options.timeout_ms = 1500;
    options.max_retry = 2;
    if (_name_agent_channel.Init(FLAGS_name_agent_url.c_str(), &options) != 0) {
        LOG(FATAL) << "[!] Fail to init channel to " << FLAGS_name_agent_url;
    }
    _name_agent_stub.reset(new name_agent::AgentService_Stub(&_name_agent_channel));
}

int NameAgentClient::GetServers(const char* service_name, std::vector<brpc::ServerNode>* servers) {
    std::string s(service_name);
    size_t p = s.find(':');
    std::string real_service_name = s.substr(0, p);
    uint32_t group_strategy = std::strtoul(s.substr(p + 1).c_str(), nullptr, 10);

    brpc::Controller cntl;
    name_agent::GetServersReq req;
    name_agent::GetServersRes res;
    req.set_seq_id(butil::gettimeofday_ms());
    req.set_service_name(real_service_name);
    req.set_group_strategy(group_strategy);
    _name_agent_stub->GetServers(&cntl, &req, &res, nullptr);
    if (!cntl.Failed()) {
        for (const std::string& endpoint : res.endpoints()) {
            butil::EndPoint ep;
            butil::str2endpoint(endpoint.c_str(), &ep);
            brpc::ServerNode node(ep);
            servers->emplace_back(node);
        }
        return 0;
    } else {
        LOG(ERROR) << "[!] NameAgentClient GetServers Error: " << cntl.ErrorText();
        return -1;
    }
}

void NameAgentClient::LbStatReport(const name_agent::LbStatReportReq& req) {
    // report async
    auto done = new OnRPCDone<name_agent::LbStatReportRes>([req](bool is_failed, name_agent::LbStatReportRes* res) {
        if (is_failed || res->res_code() != name_agent::ResCode::Success) {
            LOG(ERROR) << "LbStatReport Failed, Req:" << req.ShortDebugString();
        }
    });
    _name_agent_stub->LbStatReport(&done->cntl, &req, &done->response, done);
}