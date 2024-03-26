#pragma once

#include <sw/redis++/redis++.h>
#include "butil/memory/singleton.h"
#include "redis_wrapper.h"

namespace server {
namespace redis {

using namespace sw::redis;

class RedisManager {
public:
    void Init();
    static RedisManager* GetInstance() {
        return Singleton<RedisManager>::get();
    }

    RedisWrapper* GetRedisConnection(std::string_view cluster_name);
};

}  // namespace redis
}  // namespace server
