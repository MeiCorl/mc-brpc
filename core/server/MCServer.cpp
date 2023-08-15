#include "MCServer.h"
#include "core/utils/singleton.h"

#include <butil/files/file_path.h>
#include <butil/file_util.h>
#include <bthread/unstable.h>
#include <etcd/Client.hpp>

using namespace server;

DEFINE_string(listen_addr,
              "",
              "Server listen address, may be IPV4/IPV6/UDS."
              "If this is set, the flag port will be ignored");

MCServer::MCServer(int argc, char* argv[]) { LoggingInit(argv); }

MCServer::~MCServer() {
    if (_log_watcher) {
        delete _log_watcher;
        _log_watcher = nullptr;
    }

    if (_log_archive_worker) {
        delete _log_archive_worker;
        _log_archive_worker = nullptr;
    }

    UnRegisterService();
}

/**
 * 初始化日志文件及日志监听线程、日志归档线程
 * @date: 2023-08-14 17:08:23
 * @author: meicorl
*/
void MCServer::LoggingInit(char* argv[]) {
    logging::LoggingSettings log_settings;
    ServerConfig* svr_config = utils::Singleton<ServerConfig>::get();

    log_settings.logging_dest = logging::LOG_TO_FILE;
    if (svr_config->GetLogConfig().log_to_stderr()) {
        log_settings.logging_dest = logging::LOG_TO_ALL;
    }

    std::string log_name  = server::logger::get_log_name(argv[0]);
    log_settings.log_file = log_name.c_str();

    butil::FilePath log_path = butil::FilePath(server::logger::get_log_path(argv[0]));
    butil::CreateDirectory(log_path);

    logging::InitLogging(log_settings);

    _log_watcher = new server::logger::LogRotateWatcher(log_name);
    _log_watcher->Start();

    _log_archive_worker = new server::logger::LogArchiveWorker(log_name);
    _log_archive_worker->Start();
}

void MCServer::RegisterService() {
    ServerConfig* config = utils::Singleton<ServerConfig>::get();
    etcd::Client etcd(config->GetNsUrl());
    server::config::InstanceInfo instance;
    instance.set_region_id(config->GetSelfRegionId());
    instance.set_group_id(config->GetSelfGroupId());
    instance.set_endpoint(butil::endpoint2str(_server.listen_address()).c_str());
    etcd::Response resp = etcd.leasegrant(REGISTER_TTL).get();
    if (resp.error_code() != 0) {
        LOG(ERROR) << "[!] etcd failed, err_code: " << resp.error_code()
                   << ", err_msg:" << resp.error_message();
        exit(1);
    }

    _ns_lease_id = resp.value().lease();
    etcd::Response response =
        etcd.set(config->GetSelfName(), instance.SerializeAsString(), _ns_lease_id).get();
    if (response.error_code() != 0) {
        LOG(ERROR) << "[!] Fail to register service, err_code: " << response.error_code()
                   << ", err_msg:" << response.error_message();
        exit(1);
    }

    // 启动续约定时任务
    _service_release_timer.Init(this, REGISTER_TTL - 10);
    _service_release_timer.Start();
}

void MCServer::UnRegisterService() { }

/**
 * 定期心跳续期服务注册信息
 * @date: 2023-08-14 17:08:23
 * @author: meicorl
*/
bool MCServer::LeaseRegisteration() {
    ServerConfig* config = utils::Singleton<ServerConfig>::get();
    // etcd::Client etcd(config->GetNsUrl());

    // std::function<void(std::exception_ptr)> handler = [](std::exception_ptr eptr) {
    //     try {
    //         if (eptr) {
    //             std::rethrow_exception(eptr);
    //         }
    //     } catch (const std::runtime_error& e) {
    //         std::cerr << "Connection failure \"" << e.what() << "\"\n";
    //     } catch (const std::out_of_range& e) {
    //         std::cerr << "Lease expiry \"" << e.what() << "\"\n";
    //     }
    // };
    // etcd::KeepAlive keepalive(etcd, handler, REGISTER_TTL, _ns_lease_id);
    LOG(INFO) << "[+] Service leased, service_name:" << config->GetSelfName();
    return true;
}

void MCServer::AddService(google::protobuf::Service* service) {
    if (_server.AddService(service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "[!] Fail to add service";
        exit(1);
    }
}

void MCServer::Start() {
    butil::EndPoint point;
    if (!FLAGS_listen_addr.empty()) {
        butil::str2endpoint(FLAGS_listen_addr.c_str(), &point);
    } else {
        butil::str2endpoint("", 0, &point);
    }

    brpc::ServerOptions options;
    if (_server.Start(point, &options) != 0) {
        LOG(ERROR) << "[!] Fail to start Server";
        exit(1);
    }

    RegisterService();

    _server.RunUntilAskedToQuit();
}