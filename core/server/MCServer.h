#pragma once

#include <brpc/server.h>
#include <butil/logging.h>
#include <etcd/KeepAlive.hpp>
#include "core/config/server_config.h"
#include "core/log/log_rotate_watcher.h"
#include "core/log/log_archive_worker.h"
namespace server {

const static uint32_t REGISTER_TTL = 30;
using server::config::ServerConfig;

class MCServer {
public:
    MCServer(int argc, char* argv[]);
    ~MCServer();

    void AddService(google::protobuf::Service* service);
    void Start();

private:
    brpc::Server _server;
    uint64_t _etcd_lease_id;
    std::shared_ptr<etcd::KeepAlive> _keep_live_ptr;

    server::logger::LogRotateWatcher* _log_watcher;
    server::logger::LogArchiveWorker* _log_archive_worker;

    void LoggingInit(char* argv[]);
    void RegisterNamingService();

    std::string BuildServiceName(const std::string& original_service_name,
                                 const server::config::InstanceInfo& instance);
    void RegisterService();
    void UnRegisterService();
};

} // namespace server
