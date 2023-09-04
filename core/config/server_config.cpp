#include "server_config.h"
#include <fcntl.h>
#include <butil/fd_guard.h>
#include <butil/logging.h>
#include <gflags/gflags.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

using namespace server::config;

DEFINE_string(config_file, "../conf/server.conf", "Server config file");

ServerConfig::ServerConfig(/* args */) {
    int fd = ::open(FLAGS_config_file.c_str(), O_RDONLY);
    if (fd == -1) {
        LOG(ERROR) << "[!] Can not open server.conf";
        return;
    }
    butil::fd_guard fdGuard(fd);

    google::protobuf::io::FileInputStream* input = new google::protobuf::io::FileInputStream(fd);
    bool status = google::protobuf::TextFormat::Parse(input, &_config);
    if (!status) {
        LOG(ERROR) << "[!] Can not paser server.conf";
        return;
    }
    delete input;
    input = nullptr;
}

ServerConfig::~ServerConfig() { }

// const DbConfig& ServerConfig::GetDbConfig() { }

// const RedisConfig& ServerConfig::GetRedisConfig() { }

const LogConfig& ServerConfig::GetLogConfig() { return _config.log_config(); }

uint32_t ServerConfig::GetSelfRegionId() { return _config.region_id(); }

uint32_t ServerConfig::GetSelfGroupId() { return _config.group_id(); }

const std::string& ServerConfig::GetSelfName() { return _config.service_name(); }

const std::string& ServerConfig::GetNsUrl() { return _config.ns_url(); }
