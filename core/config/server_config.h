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

/**
 * 机房路由策略
*/
enum GroupStrategy {
    // 先本机房IP列表路由，本机房无实例再本大区路由，无实例则路由失败
    STRATEGY_NORMAL = 0,

    // 直接本大区IP列表路由，无实例则路由失败
    STRATEGY_GROUPS_ONE_REGION = 1,

    // 只在本机房路由，无实例则路由失败
    STRATEGY_SELF_GROUP = 2,

    // 先对本大区的所有机房做一致性HASH决定所返回的IP列表的机房, 再返回机房的实例IP列表路由(需要制定group_request_code)
    STRATEGY_CHASH_GROUPS = 3
};

/**
 * 实例路由策略
*/
enum LbStrategy { rr = 0, c_murmurhash = 1 };

} // namespace config
} // namespace server
