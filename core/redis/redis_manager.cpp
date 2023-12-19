#include <chrono>
#include "redis_manager.h"
#include "brpc/extension.h"
#include "core/config/server_config.h"

using namespace server::redis;
using namespace server::config;

void redisManager::Init() {
    const google::protobuf::Map<std::string, RedisConfig>& redis_confs =
        server::utils::Singleton<ServerConfig>::get()->GetRedisConfig();
    for (auto it = redis_confs.begin(); it != redis_confs.end(); ++it) {
        ConnectionOptions conn_options;
        conn_options.host = it->second.host();
        conn_options.port = it->second.port();
        conn_options.socket_timeout =
            std::chrono::milliseconds(it->second.timeout_ms() > 0 ? it->second.timeout_ms() : 1500);
        conn_options.password = it->second.passwd();

        ConnectionPoolOptions pool_options;
        pool_options.wait_timeout =
            std::chrono::milliseconds(it->second.wait_timeout_ms() > 0 ? it->second.wait_timeout_ms() : 1000);
        pool_options.size = it->second.pool_size() > 0 ? it->second.pool_size() : 3;

        // 注册集群信息
        brpc::Extension<RedisCluster>::instance()->RegisterOrDie(
            it->first, new RedisCluster(conn_options, pool_options));
    }
}

RedisCluster* redisManager::GetRedisConnection(std::string_view cluster_name) {
    RedisCluster* cluster = brpc::Extension<RedisCluster>::instance()->Find(cluster_name.data());
    if (cluster == nullptr) {
        LOG(ERROR) << "[!] db not found: " << cluster_name;
        return nullptr;
    }
    return cluster;
}

// RedisCluster* redisManager::GetRedisConnection(const std::string& cluster_name) {
//     return GetRedisConnection(cluster_name.c_str());
// }