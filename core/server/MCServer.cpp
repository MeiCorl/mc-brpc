#include "MCServer.h"
#include "core/utils/singleton.h"
#include "core/utils/net/net_utils.h"
#include <butil/files/file_path.h>
#include <butil/file_util.h>
#include <bthread/unstable.h>
#include <etcd/Client.hpp>
#include "core/extensions/mc_naming_service.h"

using namespace server;
using server::utils::NetUtil;

DEFINE_string(listen_addr,
              "",
              "Server listen address, may be IPV4/IPV6/UDS."
              "If this is set, the flag port will be ignored");

MCServer::MCServer(int argc, char* argv[]) {
    LoggingInit(argv);
    RegisterNamingService();
}

MCServer::~MCServer() {
    if (_log_watcher) {
        delete _log_watcher;
        _log_watcher = nullptr;
    }

    if (_log_archive_worker) {
        delete _log_archive_worker;
        _log_archive_worker = nullptr;
    }

    _keep_live_ptr->Cancel();
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

    _log_archive_worker =
        new server::logger::LogArchiveWorker(log_name, svr_config->GetLogConfig().remain_days());
    _log_archive_worker->Start();
}

// 注册扩展的名字服务以支持 mc://service_name
void MCServer::RegisterNamingService() {
    brpc::NamingServiceExtension()->RegisterOrDie("mc", new brpc::policy::McNamingService());
}

std::string MCServer::BuildServiceName(const std::string& original_service_name,
                                       const server::config::InstanceInfo& instance) {
    std::hash<std::string> hasher;
    return original_service_name + ":" + std::to_string(hasher(instance.SerializeAsString()));
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
    _etcd_lease_id          = resp.value().lease();
    etcd::Response response = etcd.set(BuildServiceName(config->GetSelfName(), instance),
                                       instance.SerializeAsString(), _etcd_lease_id)
                                  .get();
    if (response.error_code() != 0) {
        LOG(ERROR) << "[!] Fail to register service, err_code: " << response.error_code()
                   << ", err_msg:" << response.error_message();
        exit(1);
    }

    std::function<void(std::exception_ptr)> handler = [](std::exception_ptr eptr) {
        try {
            if (eptr) {
                std::rethrow_exception(eptr);
            }
        } catch (const std::runtime_error& e) {
            LOG(FATAL) << "[!] Etcd keepalive failure: " << e.what();
        } catch (const std::out_of_range& e) {
            LOG(FATAL) << "[!] Etcd lease expire: " << e.what();
        }
    };
    _keep_live_ptr.reset(
        new etcd::KeepAlive(config->GetNsUrl(), handler, REGISTER_TTL, _etcd_lease_id));
    LOG(INFO) << "[+] Service register succ. instance: {" << instance.ShortDebugString()
              << "}, lease_id:" << _etcd_lease_id;
}

void MCServer::UnRegisterService() {
    ServerConfig* config = utils::Singleton<ServerConfig>::get();
    server::config::InstanceInfo instance;
    instance.set_region_id(config->GetSelfRegionId());
    instance.set_group_id(config->GetSelfGroupId());
    instance.set_endpoint(butil::endpoint2str(_server.listen_address()).c_str());
    etcd::Client etcd(config->GetNsUrl());
    etcd.rm(BuildServiceName(config->GetSelfName(), instance));
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
#ifdef LOCAL_TEST
        butil::str2endpoint("", 0, &point);
#else
        char ip[32] = {0};
        NetUtil::getLocalIP(ip);
        butil::str2endpoint(ip, 0, &point);
#endif
    }

    brpc::ServerOptions options;
    if (_server.Start(point, &options) != 0) {
        LOG(ERROR) << "[!] Fail to start Server";
        exit(1);
    }

    RegisterService();

    _server.RunUntilAskedToQuit();
}