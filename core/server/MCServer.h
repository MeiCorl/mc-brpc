#pragma once

#include "brpc/server.h"
#include "core/config/server_config.h"
#include "core/log/log_rotate_watcher.h"
#include "core/log/log_archive_worker.h"
#include <etcd/KeepAlive.hpp>
namespace server {

const static uint32_t REGISTER_TTL = 30;

using server::config::ServerConfig;
using server::logger::LogArchiveWorker;
using server::logger::LogRotateWatcher;

class MCServer {
public:
    MCServer(int argc, char* argv[]);
    ~MCServer();

    void AddService(google::protobuf::Service* service);
    void Start(bool register_service = true);

private:
    brpc::Server _server;

    uint64_t _etcd_lease_id;
    std::shared_ptr<etcd::KeepAlive> _keep_live_ptr;

    std::shared_ptr<LogRotateWatcher> _log_watcher;
    std::shared_ptr<LogArchiveWorker> _log_archive_worker;

    void LoggingInit(char* argv[]);
    void RegisterNamingService();

    std::string BuildServiceName(const std::string& original_service_name,
                                 const server::config::InstanceInfo& instance);
    void RegisterService();
    void UnRegisterService();
};

} // namespace server
