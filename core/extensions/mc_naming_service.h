
#ifndef BRPC_POLICY_MC_NAMING_SERVICE
#define BRPC_POLICY_MC_NAMING_SERVICE

#include <brpc/periodic_naming_service.h>

namespace brpc {
namespace policy {

class McNamingService : public brpc::PeriodicNamingService {
    int GetServers(const char* service_name, std::vector<ServerNode>* servers) override;

    brpc::NamingService* New() const override;

    void Destroy();
};

}  // namespace policy
}  // namespace brpc

#endif