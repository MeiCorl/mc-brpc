#pragma once

#include "service_register.h"
#include "core/config/server_config.h"
#include <etcd/KeepAlive.hpp>

namespace brpc {
namespace policy {

using server::config::ServerConfig;

class EtcdServiceRegister : public ServiceRegister {
private:
    const static uint32_t REGISTER_TTL = 30;

    std::string _register_key;
    uint64_t _etcd_lease_id;
    std::shared_ptr<etcd::KeepAlive> _keep_live_ptr;

    std::string BuildServiceName(
        const std::string& original_service_name,
        const server::config::InstanceInfo& instance);

public:
    virtual bool RegisterService() override;
    virtual void UnRegisterService() override;

    void TryRegisterAgain();
};

}  // namespace policy
}  // namespace brpc