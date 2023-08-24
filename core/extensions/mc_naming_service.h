
#ifndef BRPC_POLICY_MC_NAMING_SERVICE
#define BRPC_POLICY_MC_NAMING_SERVICE

#include <brpc/periodic_naming_service.h>
#include <brpc/channel.h>

#include "name_agent.pb.h"

namespace brpc {
namespace policy {

class McNamingService : public brpc::PeriodicNamingService {
public:
    McNamingService();

private:
    int GetServers(const char* service_name, std::vector<ServerNode>* servers);

    brpc::NamingService* New() const override;

    void Destroy();

    bool _name_agent_inited;
    brpc::Channel _name_agent_channel;
    std::shared_ptr<name_agent::AgentService_Stub> _name_agent_stub;
};

} // namespace policy
} // namespace brpc

#endif