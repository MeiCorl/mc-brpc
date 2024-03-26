#include "MCServer.h"
#include "core/utils/singleton.h"
#include "core/utils/net_utils.h"
#include "butil/files/file_path.h"
#include "butil/file_util.h"
#include "bthread/unstable.h"
#include "core/extensions/mc_naming_service.h"
#include "core/extensions/lb_stat.h"

#if defined(USE_ASYNC_LOGSINK)
#include "core/log/async_logsink.h"
#elif defined(USE_FAST_LOGSINK)
#include "core/log/fast_logsink.h"
#endif

#ifdef USE_MYSQL
#include "core/mysql/db_manager.h"
#endif

#ifdef USE_REDIS
#include "core/redis/redis_manager.h"
#endif

using namespace server;
using server::utils::NetUtil;

DEFINE_string(gflags_path, "../conf/gflags.conf", "path of gflags.conf");

DEFINE_string(
    listen_addr,
    "",
    "Server listen address, may be IPV4/IPV6/UDS."
    "If this is set, the flag port will be ignored");

MCServer::MCServer(int argc, char* argv[]) {
    // 解析gflags
    if (FILE* file = fopen(FLAGS_gflags_path.c_str(), "r")) {
        google::SetCommandLineOption("flagfile", FLAGS_gflags_path.c_str());
        fclose(file);
    }

    // 创建brpc server
    _server = new brpc::Server(ServerConfig::GetInstance()->GetSelfName());

    // 初始化日志（会额外触发server.conf全局配置解析)
    LoggingInit(argv);

    // 注册名字服务
    RegisterNamingService();

    // 初始化服务注册器(默认使用EtcdServiceRegister)
    _service_register.reset(new brpc::policy::EtcdServiceRegister);

#ifdef USE_MYSQL
    // init db if necessary
    server::db::DBManager::GetInstance()->Init();
#endif

#ifdef USE_REDIS
    // init redis cluster if necessary
    server::redis::RedisManager::GetInstance()->Init();
#endif
}

MCServer::~MCServer() {
    // 取消服务注册
    if (_service_register != nullptr) {
        _service_register->UnRegisterService();
    }

    // 停止lb上报线程
    brpc::policy::LbStat::GetInstance()->Stop();

    // 停止日志watcher
    _log_watcher.reset();
    _log_archive_worker.reset();
#if defined(USE_ASYNC_LOGSINK)
    logging::LogSink* old_sink = logging::SetLogSink(nullptr);
    if (old_sink) {
        delete old_sink;
    }
#elif defined(USE_FAST_LOGSINK)
    logging::LogSink* old_sink = logging::SetLogSink(nullptr);
    if (old_sink) {
        delete old_sink;
    }
#endif

    // 销毁brpc::Server
    if (_server) {
        delete _server;
        _server = nullptr;
    }
}

/**
 * 初始化日志文件及日志监听线程、日志归档线程
 * @date: 2023-08-14 17:08:23
 * @author: meicorl
 */
void MCServer::LoggingInit(char* argv[]) {
    ServerConfig* svr_config = ServerConfig::GetInstance();

    logging::LoggingSettings log_settings;
    log_settings.logging_dest = logging::LOG_TO_FILE;
    if (svr_config->GetLogConfig().log_to_stderr()) {
        log_settings.logging_dest = logging::LOG_TO_ALL;
    }

    std::string log_name = server::logger::get_log_name(argv[0]);
    log_settings.log_file = log_name.c_str();

    butil::FilePath log_path = butil::FilePath(server::logger::get_log_path(argv[0]));
    butil::CreateDirectory(log_path);

#if defined(USE_ASYNC_LOGSINK)
    logging::LogSink* log_sink = new server::logger::AsyncLogSink(log_settings);
    logging::LogSink* old_sink = logging::SetLogSink(log_sink);
    if (old_sink) {
        delete old_sink;
    }
    LOG(INFO) << "Using async_logsink...";
#elif defined(USE_FAST_LOGSINK)
    logging::LogSink* log_sink = new server::logger::FastLogSink(log_settings);
    logging::LogSink* old_sink = logging::SetLogSink(log_sink);
    if (old_sink) {
        delete old_sink;
    }
    LOG(INFO) << "Using fast_logsink...";
#else
    logging::InitLogging(log_settings);
    LOG(INFO) << "Using default_logsink...";
#endif

    _log_watcher = std::make_shared<LogRotateWatcher>(log_name);
    _log_watcher->Start();

    _log_archive_worker = std::make_shared<LogArchiveWorker>(log_name, svr_config->GetLogConfig().remain_days());
    _log_archive_worker->Start();
}

// 注册扩展的名字服务以支持 mc://service_name
void MCServer::RegisterNamingService() {
    brpc::NamingServiceExtension()->RegisterOrDie("mc", new brpc::policy::McNamingService());
}

// 替换服务注册器
void MCServer::SetServiceRegister(brpc::policy::ServiceRegister* service_register) {
    _service_register.reset(service_register);
}

void MCServer::AddService(google::protobuf::Service* service) {
    if (_server->AddService(service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "[!] Fail to add service";
        exit(1);
    }
}

void MCServer::Start(bool register_service) {
    // start brpc server
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
    options.server_info_name = ServerConfig::GetInstance()->GetSelfName();
    if (_server->Start(point, &options) != 0) {
        LOG(ERROR) << "[!] Fail to start Server";
        exit(1);
    }

    // register service if necessary
    if (register_service) {
        if (!_service_register) {
            LOG(ERROR) << "[!] service register not found!";
            exit(1);
        }

        if (!_service_register->RegisterService()) {
            exit(1);
        }
    }

    // init lb stat
    brpc::policy::LbStat::GetInstance()->Init();

    // loop
    _server->RunUntilAskedToQuit();
}