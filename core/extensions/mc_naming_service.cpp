#include "mc_naming_service.h"
#include "core/common/name_agent_client.h"
#include "brpc/controller.h"

namespace brpc {
namespace policy {

using server::common::NameAgentClient;

int McNamingService::GetServers(const char* service_name, std::vector<ServerNode>* servers) {
    return NameAgentClient::GetInstance()->GetServers(service_name, servers);
}

brpc::NamingService* McNamingService::New() const {
    return new McNamingService;
}

void McNamingService::Destroy() {
    delete this;
}

}  // namespace policy
}  // namespace brpc