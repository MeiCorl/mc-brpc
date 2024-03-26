#include "redis_manager.h"
#include "brpc/extension.h"
#include "core/config/server_config.h"

using namespace server::redis;
using namespace server::config;

void RedisManager::Init() {
    const google::protobuf::Map<std::string, RedisConfig>& redis_confs = ServerConfig::GetInstance()->GetRedisConfig();
    for (auto it = redis_confs.begin(); it != redis_confs.end(); ++it) {
        if (it->second.has_redis_info()) {
            ConnectionOptions conn_options;
            conn_options.host = it->second.redis_info().host();
            conn_options.port = it->second.redis_info().port();
            conn_options.socket_timeout =
                std::chrono::milliseconds(it->second.timeout_ms() > 0 ? it->second.timeout_ms() : 1500);
            conn_options.password = it->second.passwd();

            ConnectionPoolOptions pool_options;
            pool_options.wait_timeout =
                std::chrono::milliseconds(it->second.wait_timeout_ms() > 0 ? it->second.wait_timeout_ms() : 1000);
            pool_options.size = it->second.pool_size() > 0 ? it->second.pool_size() : 3;

            // 注册Cluster/Redis信息
            if (it->second.redis_info().type() == "cluster") {
                RedisCluster* cluster = new RedisCluster(conn_options, pool_options);
                brpc::Extension<RedisWrapper>::instance()->RegisterOrDie(it->first, new RedisWrapper(cluster));
                LOG(INFO) << "redis cluster inited: " << it->first;
            } else {
                Redis* redis = new Redis(conn_options, pool_options);
                brpc::Extension<RedisWrapper>::instance()->RegisterOrDie(it->first, new RedisWrapper(redis));
                LOG(INFO) << "single redis inited: " << it->first;
            }
        } else if (it->second.has_sentine_info()) {
            SentinelOptions sentinel_opts;
            for (auto& sentine : it->second.sentine_info().sentines()) {
                sentinel_opts.nodes.emplace_back(make_pair(sentine.host(), sentine.port()));
            }
            sentinel_opts.socket_timeout =
                std::chrono::milliseconds(it->second.timeout_ms() > 0 ? it->second.timeout_ms() : 1500);
            auto sentinel = std::make_shared<Sentinel>(sentinel_opts);

            ConnectionOptions conn_options;
            conn_options.socket_timeout =
                std::chrono::milliseconds(it->second.timeout_ms() > 0 ? it->second.timeout_ms() : 1500);
            conn_options.password = it->second.passwd();

            ConnectionPoolOptions pool_options;
            pool_options.wait_timeout =
                std::chrono::milliseconds(it->second.wait_timeout_ms() > 0 ? it->second.wait_timeout_ms() : 1000);
            pool_options.size = it->second.pool_size() > 0 ? it->second.pool_size() : 3;

            // 注册Redis信息
            Redis* redis = new Redis(sentinel, "master_name", Role::MASTER, conn_options, pool_options);
            brpc::Extension<RedisWrapper>::instance()->RegisterOrDie(it->first, new RedisWrapper(redis));
            LOG(INFO) << "redis sentine inited: " << it->first;
        }
    }
}

RedisWrapper* RedisManager::GetRedisConnection(std::string_view cluster_name) {
    RedisWrapper* cluster = brpc::Extension<RedisWrapper>::instance()->Find(cluster_name.data());
    if (cluster == nullptr) {
        LOG(ERROR) << "[!] db not found: " << cluster_name;
        return nullptr;
    }
    return cluster;
}

// RedisCluster* RedisManager::GetRedisConnection(const std::string& cluster_name) {
//     return GetRedisConnection(cluster_name.c_str());
// }