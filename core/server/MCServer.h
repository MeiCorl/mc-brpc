#pragma once

#include "brpc/server.h"
#include "core/config/server_config.h"
#include "core/log/log_rotate_watcher.h"
#include "core/log/log_archive_worker.h"
#include "core/extensions/etcd_service_register.h"
namespace server {

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
    brpc::Server* _server;

    std::shared_ptr<LogRotateWatcher> _log_watcher;
    std::shared_ptr<LogArchiveWorker> _log_archive_worker;
    std::shared_ptr<brpc::policy::ServiceRegister> _service_register;

    void LoggingInit(char* argv[]);
    void RegisterNamingService();
    void SetServiceRegister(brpc::policy::ServiceRegister* service_register);
};

} // namespace server
