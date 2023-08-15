#pragma once

#include <brpc/server.h>
#include <butil/logging.h>
#include "core/config/server_config.h"
#include "core/log/log_rotate_watcher.h"
#include "core/log/log_archive_worker.h"
#include "core/utils/simple_timer_task.h"

namespace server {

const static uint32_t REGISTER_TTL = 60;
using server::config::ServerConfig;

class MCServer {
public:
    MCServer(int argc, char* argv[]);
    ~MCServer();

    void AddService(google::protobuf::Service* service);
    void Start();

    bool LeaseRegisteration();

private:
    brpc::Server _server;

    int64_t _ns_lease_id; // 服务注册到etcd的租约id, 需要通过心跳定期续租, 否则租约到期注册信息将从etcd删除
    server::utils::SimpleTimerTask<MCServer, &MCServer::LeaseRegisteration> _service_release_timer;

    server::logger::LogRotateWatcher* _log_watcher;
    server::logger::LogArchiveWorker* _log_archive_worker;

    void LoggingInit(char* argv[]);

    void RegisterService();
    void UnRegisterService();
};

} // namespace server
