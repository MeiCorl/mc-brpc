#pragma once

#include "brpc/channel.h"
#include "butil/memory/singleton.h"
#include "core/extensions/name_agent.pb.h"

namespace server {
namespace common {

class NameAgentClient {
public:
    static NameAgentClient* GetInstance();
    NameAgentClient();

    int GetServers(const char* service_name, std::vector<brpc::ServerNode>* servers);
    void LbStatReport(const name_agent::LbStatReportReq& req);
private:
    brpc::Channel _name_agent_channel;
    std::shared_ptr<name_agent::AgentService_Stub> _name_agent_stub;
};

}  // namespace common
}  // namespace server