#pragma once

#include "server_config.pb.h"

namespace server {
namespace config {

/**
 * 服务全局配置
*/
class ServerConfig {
private:
    SvrConfigBase _config;

public:
    ServerConfig(/* args */);
    ~ServerConfig();

    // const DbConfig& GetDbConfig();
    // const RedisConfig& GetRedisConfig();
    const LogConfig& GetLogConfig();

    uint32_t GetSelfRegionId();
    uint32_t GetSelfGroupId();
    const std::string& GetSelfName();
    const std::string& GetNsUrl();
};


} // namespace config
} // namespace server
