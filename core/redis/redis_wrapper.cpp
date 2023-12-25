#include "redis_wrapper.h"

using namespace server::redis;

RedisWrapper::RedisWrapper(Redis *redis) : p_instance(redis) {}

RedisWrapper::RedisWrapper(RedisCluster *cluster) : p_instance(cluster) {}

RedisWrapper::~RedisWrapper() {}

bool RedisWrapper::set(
    const StringView &key,
    const StringView &val,
    const std::chrono::milliseconds &ttl,
    UpdateType type) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->set(key, val, ttl, type);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->set(key, val, ttl, type);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::del(const StringView &key) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->del(key);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->del(key);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::exists(const StringView &key) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->exists(key);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->exists(key);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

bool RedisWrapper::expire(const StringView &key, const std::chrono::seconds &timeout) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->expire(key, timeout);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->expire(key, timeout);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::lpush(const StringView &key, const StringView &val) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->lpush(key, val);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->lpush(key, val);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

OptionalString RedisWrapper::lpop(const StringView &key) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->lpop(key);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->lpop(key);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::llen(const StringView &key) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->llen(key);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->llen(key);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::lrem(const StringView &key, long long count, const StringView &val) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->lrem(key, count, val);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->lrem(key, count, val);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

void RedisWrapper::ltrim(const StringView &key, long long start, long long stop) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->ltrim(key, start, stop);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->ltrim(key, start, stop);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::rpush(const StringView &key, const StringView &val) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->rpush(key, val);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->rpush(key, val);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

OptionalString RedisWrapper::rpop(const StringView &key) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->rpop(key);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->rpop(key);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::hdel(const StringView &key, const StringView &field) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->hdel(key, field);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->hdel(key, field);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::hset(const StringView &key, const StringView &field, const StringView &val) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->hset(key, field, val);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->hset(key, field, val);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::hset(const StringView &key, const std::pair<StringView, StringView> &item) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->hset(key, item.first, item.second);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->hset(key, item.first, item.second);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

OptionalString RedisWrapper::hget(const StringView &key, const StringView &field) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->hget(key, field);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->hget(key, field);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::hlen(const StringView &key) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->hlen(key);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->hlen(key);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::hincrby(const StringView &key, const StringView &field, long long increment) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->hincrby(key, field, increment);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->hincrby(key, field, increment);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

double RedisWrapper::hincrbyfloat(const StringView &key, const StringView &field, double increment) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->hincrby(key, field, increment);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->hincrby(key, field, increment);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::sadd(const StringView &key, const StringView &member) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->sadd(key, member);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->sadd(key, member);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::scard(const StringView &key) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->scard(key);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->scard(key);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

bool RedisWrapper::sismember(const StringView &key, const StringView &member) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->sismember(key, member);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->sismember(key, member);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::srem(const StringView &key, const StringView &member) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->srem(key, member);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->srem(key, member);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::zadd(
    const StringView &key,
    const StringView &member,
    double score,
    UpdateType type,
    bool changed) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->zadd(key, member, score, type, changed);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->zadd(key, member, score, type, changed);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

double RedisWrapper::zincrby(const StringView &key, double increment, const StringView &member) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->zincrby(key, increment, member);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->zincrby(key, increment, member);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

OptionalDouble RedisWrapper::zscore(const StringView &key, const StringView &member) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->zscore(key, member);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->zscore(key, member);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

bool RedisWrapper::pfadd(const StringView &key, const StringView &element) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->pfadd(key, element);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->pfadd(key, element);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::pfcount(const StringView &key) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->pfcount(key);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->pfcount(key);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

void RedisWrapper::pfmerge(const StringView &destination, const StringView &key) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->pfmerge(destination, key);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->pfmerge(destination, key);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

long long RedisWrapper::publish(const StringView &channel, const StringView &message) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->publish(channel, message);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->publish(channel, message);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}

Pipeline RedisWrapper::pipeline(bool new_connection, const StringView &hash_tag) {
    if (std::holds_alternative<Redis *>(p_instance)) {
        return std::get<Redis *>(p_instance)->pipeline(new_connection);
    } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
        return std::get<RedisCluster *>(p_instance)->pipeline(hash_tag, new_connection);
    } else {
        throw std::runtime_error("Redis p_instance not found.");
    }
}