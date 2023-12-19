#pragma once

#include <sw/redis++/redis++.h>
#include "core/utils/singleton.h"

namespace server {
namespace redis {

using namespace sw::redis;

class redisManager {
public:
    void Init();

    RedisCluster* GetRedisConnection(std::string_view cluster_name);
};

}  // namespace redis
}  // namespace server

using RedisManager = server::utils::Singleton<server::redis::redisManager>;