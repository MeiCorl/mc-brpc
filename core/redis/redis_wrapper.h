/**
 * Wrapper class for Redis and RedisCluster
 * @author: meicorl
 * @date: 2023/12/25 10:59:00
 */
#pragma once

#include <variant>
#include <sw/redis++/redis++.h>
#include <chrono>
#include <stdexcept>

namespace server {
namespace redis {

using namespace sw::redis;

class RedisWrapper {
private:
    std::variant<Redis *, RedisCluster *> p_instance;

public:
    RedisWrapper(Redis *redis);
    RedisWrapper(RedisCluster *cluster);
    ~RedisWrapper();

    /**
     * @param key Key.
     * @param val Value.
     * @param ttl Timeout on the key. If `ttl` is 0ms, do not set timeout.
     * @param type Options for set command:
     *             - UpdateType::EXIST: Set the key only if it already exists.
     *             - UpdateType::NOT_EXIST: Set the key only if it does not exist.
     *             - UpdateType::ALWAYS: Always set the key no matter whether it exists.
     * @return Whether the key has been set.
     * @retval true If the key has been set.
     * @retval false If the key was not set, because of the given option.
     */
    bool set(
        const StringView &key,
        const StringView &val,
        const std::chrono::milliseconds &ttl = std::chrono::milliseconds(0),
        UpdateType type = UpdateType::ALWAYS);

    /**
     * @brief Delete the given key.
     * @param key Key.
     * @return Number of keys removed.
     */
    long long del(const StringView &key);

    /**
     * @brief Check if the given key exists.
     * @param key Key.
     * @return Whether the given key exists.
     * @retval 1 If key exists.
     * @retval 0 If key does not exist.
     */
    long long exists(const StringView &key);

    /**
     * @brief Set a timeout on key.
     * @param key Key.
     * @param timeout Timeout in seconds.
     * @return Whether timeout has been set.
     * @retval true If timeout has been set.
     * @retval false If key does not exist.
     */
    bool expire(const StringView &key, const std::chrono::seconds &timeout);

    /**
     * @brief Push an element to the beginning of the list.
     * @param key Key where the list is stored.
     * @param val Element to be pushed.
     * @return The length of the list after the operation.
     */
    long long lpush(const StringView &key, const StringView &val);

    /**
     * @brief Push multiple elements to the beginning of the list.
     *
     * Example:
     * @code{.cpp}
     *      std::vector<std::string> elements = {"e1", "e2", "e3"};
     *      redis.lpush("list", elements.begin(), elements.end());
     * @endcode
     * @param key Key where the list is stored.
     * @param first Iterator to the first element to be pushed.
     * @param last Off-the-end iterator to the given element range.
     * @return The length of the list after the operation.
     */
    template <typename Input>
    long long lpush(const StringView &key, Input first, Input last) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->lpush(key, first, last);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->lpush(key, first, last);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Push multiple elements to the beginning of the list.
     *
     * Example:
     * @code{.cpp}
     *      redis.lpush("list", {"e1", "e2", "e3"});
     * @endcode
     * @param key Key where the list is stored.
     * @param il Initializer list of elements.
     * @return The length of the list after the operation.
     */
    template <typename T>
    long long lpush(const StringView &key, std::initializer_list<T> il) {
        return lpush(key, il.begin(), il.end());
    }

    /**
     * @brief Pop the first element of the list.
     *
     * Example:
     * @code{.cpp}
     *       auto element = redis.lpop("list");
     *       if (element)
     *           std::cout << *element << std::endl;
     *       else
     *           std::cout << "list is empty, i.e. list does not exist" << std::endl;
     * @endcode
     * @param key Key where the list is stored.
     * @return The popped element.
     * @note If list is empty, i.e. key does not exist, return `OptionalString{}` (`std::nullopt`).
     */
    OptionalString lpop(const StringView &key);

    /**
     * @brief Get the length of the list.
     * @param key Key where the list is stored.
     * @return The length of the list.
     */
    long long llen(const StringView &key);

    /**
     * @brief Get elements in the given range of the given list.
     *
     * Example:
     * @code{.cpp}
     *       std::vector<std::string> elements;
     *       // Save all elements of a Redis list to a vector of string.
     *       redis.lrange("list", 0, -1, std::back_inserter(elements));
     * @endcode
     * @param key Key where the list is stored.
     * @param start Start index of the range. Index can be negative, which mean index from the end.
     * @param stop End index of the range.
     * @param output Output iterator to the destination where the results are saved.
     */
    template <typename Output>
    void lrange(const StringView &key, long long start, long long stop, Output output) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            std::get<Redis *>(p_instance)->lrange(key, start, stop, output);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            std::get<RedisCluster *>(p_instance)->lrange(key, start, stop, output);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Remove the first `count` occurrences of elements equal to `val`.
     * @param key Key where the list is stored.
     * @param count Number of occurrences to be removed.
     * @param val Value.
     * @return Number of elements removed.
     * @note `count` can be positive, negative and 0. Check the reference for detail.
     */
    long long lrem(const StringView &key, long long count, const StringView &val);

    /**
     * @brief Trim a list to keep only element in the given range.
     * @param key Key where the key is stored.
     * @param start Start of the index.
     * @param stop End of the index.
     */
    void ltrim(const StringView &key, long long start, long long stop);

    /**
     * @brief Push an element to the end of the list.
     * @param key Key where the list is stored.
     * @param val Element to be pushed.
     * @return The length of the list after the operation.
     */
    long long rpush(const StringView &key, const StringView &val);

    /**
     * @brief Push multiple elements to the end of the list.
     *
     * Example:
     * @code{.cpp}
     *      std::vector<std::string> elements = {"e1", "e2", "e3"};
     *      redis.rpush("list", elements.begin(), elements.end());
     * @endcode
     * @param key Key where the list is stored.
     * @param first Iterator to the first element to be pushed.
     * @param last Off-the-end iterator to the given element range.
     * @return The length of the list after the operation.
     */
    template <typename Input>
    long long rpush(const StringView &key, Input first, Input last) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->rpush(key, first, last);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->rpush(key, first, last);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Push multiple elements to the end of the list.
     *
     * Example:
     * @code{.cpp}
     *      redis.rpush("list", {"e1", "e2", "e3"});
     * @endcode
     * @param key Key where the list is stored.
     * @param il Initializer list of elements.
     * @return The length of the list after the operation.
     */
    template <typename T>
    long long rpush(const StringView &key, std::initializer_list<T> il) {
        return rpush(key, il.begin(), il.end());
    }

    /**
     * @brief Pop the last element of the list.
     *
     * Example:
     * @code{.cpp}
     *       auto element = redis.rpop("list");
     *       if (element)
     *           std::cout << *element << std::endl;
     *       else
     *           std::cout << "list is empty, i.e. list does not exist" << std::endl;
     * @endcode
     * @param key Key where the list is stored.
     * @return The popped element.
     * @note If list is empty, i.e. key does not exist, return `OptionalString{}` (`std::nullopt`).
     */
    OptionalString rpop(const StringView &key);

    /**
     * @brief Remove the given field from hash.
     * @param key Key where the hash is stored.
     * @param field Field to be removed.
     * @return Whether the field has been removed.
     * @retval 1 If the field exists, and has been removed.
     * @retval 0 If the field does not exist.
     */
    long long hdel(const StringView &key, const StringView &field);

    /**
     * @brief Remove multiple fields from hash.
     * @param key Key where the hash is stored.
     * @param first Iterator to the first field to be removed.
     * @param last Off-the-end iterator to the given field range.
     * @return Number of fields that has been removed.
     */
    template <typename Input>
    long long hdel(const StringView &key, Input first, Input last) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->hdel(key, first, last);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->hdel(key, first, last);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Remove multiple fields from hash.
     * @param key Key where the hash is stored.
     * @param il Initializer list of fields.
     * @return Number of fields that has been removed.
     */
    template <typename T>
    long long hdel(const StringView &key, std::initializer_list<T> il) {
        return hdel(key, il.begin(), il.end());
    }

    /**
     * @brief Set hash field to value.
     * @param key Key where the hash is stored.
     * @param field Field.
     * @param val Value.
     * @return Whether the given field is a new field.
     * @retval 1 If the given field didn't exist, and a new field has been added.
     * @retval 0 If the given field already exists, and its value has been overwritten.
     */
    long long hset(const StringView &key, const StringView &field, const StringView &val);

    /**
     * @brief Set hash field to value.
     * @param key Key where the hash is stored.
     * @param item The field-value pair to be set.
     * @return Whether the given field is a new field.
     * @retval 1 If the given field didn't exist, and a new field has been added.
     * @retval 0 If the given field already exists, and its value has been overwritten.
     */
    long long hset(const StringView &key, const std::pair<StringView, StringView> &item);

    /**
     @brief Set multiple fields of the given hash.
    *
    * Example:
    * @code{.cpp}
    *       std::unordered_map<std::string, std::string> m = {{"f1", "v1"}, {"f2", "v2"}};
    *       redis.hset("hash", m.begin(), m.end());
    * @endcode
    * @param key Key where the hash is stored.
    * @param first Iterator to the first field to be set.
    * @param last Off-the-end iterator to the given range.
    * @return Number of fields that have been added, i.e. fields that not existed before.
    */
    template <typename Input>
    auto hset(const StringView &key, Input first, Input last) ->
        typename std::enable_if<!std::is_convertible<Input, StringView>::value, long long>::type {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->hset(key, first, last);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->hset(key, first, last);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Set multiple fields of the given hash.
     *
     * Example:
     * @code{.cpp}
     *   redis.hset("hash", {std::make_pair("f1", "v1"), std::make_pair("f2", "v2")});
     * @endcode
     * @param key Key where the hash is stored.
     * @param il Initializer list of field-value pairs.
     * @return Number of fields that have been added, i.e. fields that not existed before.
     */
    template <typename T>
    long long hset(const StringView &key, std::initializer_list<T> il) {
        return hset(key, il.begin(), il.end());
    }

    /**
     * @brief Get the value of the given field.
     * @param key Key where the hash is stored.
     * @param field Field.
     * @return Value of the given field.
     * @note If field does not exist, `hget` returns `OptionalString{}` (`std::nullopt`).
     */
    OptionalString hget(const StringView &key, const StringView &field);

    /**
     * @brief Get all field-value pairs of the given hash.
     *
     * Example:
     * @code{.cpp}
     *       std::unordered_map<std::string, std::string> results;
     *       // Save all field-value pairs of a Redis hash to an unordered_map<string, string>.
     *       redis.hgetall("hash", std::inserter(results, results.begin()));
     * @endcode
     * @param key Key where the hash is stored.
     * @param output Output iterator to the destination where the result is saved.
     */
    template <typename Output>
    void hgetall(const StringView &key, Output output) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            std::get<Redis *>(p_instance)->hgetall(key, output);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            std::get<RedisCluster *>(p_instance)->hgetall(key, output);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Get values of multiple fields.
     *
     * Example:
     * @code{.cpp}
     *       std::vector<std::string> fields = {"f1", "f2"};
     *       std::vector<OptionalString> vals;
     *       redis.hmget("hash", fields.begin(), fields.end(), std::back_inserter(vals));
     *       for (const auto &val : vals) {
     *           if (val)
     *               std::cout << *val << std::endl;
     *           else
     *               std::cout << "field not exist" << std::endl;
     *       }
     * @endcode
     * @param key Key where the hash is stored.
     * @param first Iterator to the first field.
     * @param last Off-the-end iterator to the given field range.
     * @param output Output iterator to the destination where the result is saved.
     * @note The destination should be a container of `OptionalString` type,
     *       since the given field might not exist (in this case, the value of the corresponding
     *       field is `OptionalString{}` (`std::nullopt`)).
     */
    template <typename Input, typename Output>
    void hmget(const StringView &key, Input first, Input last, Output output) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            std::get<Redis *>(p_instance)->hmget(key, first, last, output);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            std::get<RedisCluster *>(p_instance)->hmget(key, first, last, output);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Get values of multiple fields.
     *
     * Example:
     * @code{.cpp}
     *       std::vector<OptionalString> vals;
     *       redis.hmget("hash", {"f1", "f2"}, std::back_inserter(vals));
     *       for (const auto &val : vals) {
     *           if (val)
     *               std::cout << *val << std::endl;
     *           else
     *               std::cout << "field not exist" << std::endl;
     *       }
     * @endcode
     * @param key Key where the hash is stored.
     * @param il Initializer list of fields.
     * @param output Output iterator to the destination where the result is saved.
     * @note The destination should be a container of `OptionalString` type,
     *       since the given field might not exist (in this case, the value of the corresponding
     *       field is `OptionalString{}` (`std::nullopt`)).
     */
    template <typename T, typename Output>
    void hmget(const StringView &key, std::initializer_list<T> il, Output output) {
        hmget(key, il.begin(), il.end(), output);
    }

    /**
     * @brief Get the number of fields of the given hash.
     * @param key Key where the hash is stored.
     * @return Number of fields.
     */
    long long hlen(const StringView &key);

    /**
     * @brief Get all fields of the given hash.
     * @param key Key where the hash is stored.
     * @param output Output iterator to the destination where the result is saved.
     * @note It's always a bad idea to call `hkeys` on a large hash, since it will block Redis.
     */
    template <typename Output>
    void hkeys(const StringView &key, Output output) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            std::get<Redis *>(p_instance)->hkeys(key, output);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            std::get<RedisCluster *>(p_instance)->hkeys(key, output);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Get values of all fields stored at the given hash.
     * @param key Key where the hash is stored.
     * @param output Output iterator to the destination where the result is saved.
     * @note It's always a bad idea to call `hvals` on a large hash, since it might block Redis.
     */
    template <typename Output>
    void hvals(const StringView &key, Output output) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            std::get<Redis *>(p_instance)->hvals(key, output);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            std::get<RedisCluster *>(p_instance)->hkeys(hvals, output);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Increment the integer stored at the given field.
     * @param key Key where the hash is stored.
     * @param field Field.
     * @param increment Increment.
     * @return The value of the field after the increment.
     */
    long long hincrby(const StringView &key, const StringView &field, long long increment);
    double hincrbyfloat(const StringView &key, const StringView &field, double increment);

    /**
     * @brief Add a member to the given set.
     * @param key Key where the set is stored.
     * @param member Member to be added.
     * @return Whether the given member is a new member.
     * @retval 1 The member did not exist before, and it has been added now.
     * @retval 0 The member already exists before this operation.
     */
    long long sadd(const StringView &key, const StringView &member);

    /**
     * @brief Add multiple members to the given set.
     * @param key Key where the set is stored.
     * @param first Iterator to the first member to be added.
     * @param last Off-the-end iterator to the member range.
     * @return Number of new members that have been added, i.e. members did not exist before.
     */
    template <typename Input>
    long long sadd(const StringView &key, Input first, Input last) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->sadd(key, first, last);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->sadd(key, first, last);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Add multiple members to the given set.
     * @param key Key where the set is stored.
     * @param il Initializer list of members to be added.
     * @return Number of new members that have been added, i.e. members did not exist before.
     */
    template <typename T>
    long long sadd(const StringView &key, std::initializer_list<T> il) {
        return sadd(key, il.begin(), il.end());
    }

    /**
     * @brief Get the number of members in the set.
     * @param key Key where the set is stored.
     * @return Number of members.
     */
    long long scard(const StringView &key);

    /**
     * @brief Test if `member` exists in the set stored at key.
     * @param key Key where the set is stored.
     * @param member Member to be checked.
     * @return Whether `member` exists in the set.
     * @retval true If it exists in the set.
     * @retval false If it does not exist in the set, or the given key does not exist.
     */
    bool sismember(const StringView &key, const StringView &member);

    /**
     * @brief Get all members in the given set.
     *
     * Example:
     * @code{.cpp}
     *       std::unordered_set<std::string> members1;
     *       redis.smembers("set", std::inserter(members1, members1.begin()));
     *       std::vector<std::string> members2;
     *       redis.smembers("set", std::back_inserter(members2));
     * @endcode
     * @param key Key where the set is stored.
     * @param output Iterator to the destination where the result is saved.
     */
    template <typename Output>
    void smembers(const StringView &key, Output output) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->smembers(key, output);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->smembers(key, output);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Remove a member from set.
     * @param key Key where the set is stored.
     * @param member Member to be removed.
     * @return Whether the member has been removed.
     * @retval 1 If the given member exists, and has been removed.
     * @retval 0 If the given member does not exist.
     */
    long long srem(const StringView &key, const StringView &member);

    /**
     * @brief Remove multiple members from set.
     * @param key Key where the set is stored.
     * @param first Iterator to the first member to be removed.
     * @param last Off-the-end iterator to the range.
     * @return Number of members that have been removed.
     */
    template <typename Input>
    long long srem(const StringView &key, Input first, Input last) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->srem(key, first, last);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->srem(key, first, last);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Remove multiple members from set.
     * @param key Key where the set is stored.
     * @param il Initializer list of members to be removed.
     * @return Number of members that have been removed.
     */
    template <typename T>
    long long srem(const StringView &key, std::initializer_list<T> il) {
        return srem(key, il.begin(), il.end());
    }

    /**
     @brief Add or update a member with score to sorted set.
    * @param key Key where the sorted set is stored.
    * @param member Member to be added.
    * @param score Score of the member.
    * @param type Options for zadd command:
    *             - UpdateType::EXIST: Add the member only if it already exists.
    *             - UpdateType::NOT_EXIST: Add the member only if it does not exist.
    *             - UpdateType::ALWAYS: Always add the member no matter whether it exists.
    * @param changed Whether change the return value from number of newly added member to
    *                number of members changed (i.e. added and updated).
    * @return Number of added members or number of added and updated members depends on `changed`.
    * @note We don't support the INCR option, because in this case, the return value of zadd
    *       command is NOT of type long long. However, you can always use the generic interface
    *       to send zadd command with INCR option:
    *       `auto score = redis.command<OptionalDouble>("ZADD", "key", "XX", "INCR", 10, "mem");`
    * @see `UpdateType`
    */
    long long zadd(
        const StringView &key,
        const StringView &member,
        double score,
        UpdateType type = UpdateType::ALWAYS,
        bool changed = false);

    /**
     * @brief Add or update multiple members with score to sorted set.
     *
     * Example:
     * @code{.cpp}
     *       std::unordered_map<std::string, double> m = {{"m1", 1.2}, {"m2", 2.3}};
     *       redis.zadd("zset", m.begin(), m.end());
     * @endcode
     * @param key Key where the sorted set is stored.
     * @param first Iterator to the first member-score pair.
     * @param last Off-the-end iterator to the member-score pairs range.
     * @param type Options for zadd command:
     *             - UpdateType::EXIST: Add the member only if it already exists.
     *             - UpdateType::NOT_EXIST: Add the member only if it does not exist.
     *             - UpdateType::ALWAYS: Always add the member no matter whether it exists.
     * @param changed Whether change the return value from number of newly added member to
     *                number of members changed (i.e. added and updated).
     * @return Number of added members or number of added and updated members depends on `changed`.
     */
    template <typename Input>
    long long zadd(
        const StringView &key,
        Input first,
        Input last,
        UpdateType type = UpdateType::ALWAYS,
        bool changed = false) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->zadd(key, first, last, type, changed);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->zadd(key, first, last, type, changed);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Add or update multiple members with score to sorted set.
     *
     * Example:
     * @code{.cpp}
     *       redis.zadd("zset", {std::make_pair("m1", 1.4), std::make_pair("m2", 2.3)});
     * @endcode
     * @param key Key where the sorted set is stored.
     * @param first Iterator to the first member-score pair.
     * @param last Off-the-end iterator to the member-score pairs range.
     * @param type Options for zadd command:
     *             - UpdateType::EXIST: Add the member only if it already exists.
     *             - UpdateType::NOT_EXIST: Add the member only if it does not exist.
     *             - UpdateType::ALWAYS: Always add the member no matter whether it exists.
     * @param changed Whether change the return value from number of newly added member to
     *                number of members changed (i.e. added and updated).
     * @return Number of added members or number of added and updated members depends on `changed`.
     */
    template <typename T>
    long long zadd(
        const StringView &key,
        std::initializer_list<T> il,
        UpdateType type = UpdateType::ALWAYS,
        bool changed = false) {
        return zadd(key, il.begin(), il.end(), type, changed);
    }

    /**
     * @brief Increment the score of given member.
     * @param key Key where the sorted set is stored.
     * @param increment Increment.
     * @param member Member.
     * @return The score of the member after the operation.
     */
    double zincrby(const StringView &key, double increment, const StringView &member);

    /**
     * @brief Get a range of members by rank (ordered from lowest to highest).
     *
     * Example:
     * @code{.cpp}
     *       // send *ZRANGE* command without the *WITHSCORES* option:
     *       std::vector<std::string> result;
     *       redis.zrange("zset", 0, -1, std::back_inserter(result));
     *       // send command with *WITHSCORES* option:
     *       std::vector<std::pair<std::string, double>> with_score;
     *       redis.zrange("zset", 0, -1, std::back_inserter(with_score));
     * @endcode
     * @param key Key where the sorted set is stored.
     * @param start Start rank. Inclusive and can be negative.
     * @param stop Stop rank. Inclusive and can be negative.
     * @param output Output iterator to the destination where the result is saved.
     * @note This method can also return the score of each member. If `output` is an iterator
     *       to a container of `std::string`, we send *ZRANGE key start stop* command.
     *       If it's an iterator to a container of `std::pair<std::string, double>`,
     *       we send *ZRANGE key start stop WITHSCORES* command. See the *Example* part on
     *       how to use this method.
     */
    template <typename Output>
    void zrange(const StringView &key, long long start, long long stop, Output output) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->zrange(key, start, stop, output);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->zrange(key, start, stop, output);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    template <typename Output>
    void zrevrange(const StringView &key, long long start, long long stop, Output output) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->zrevrange(key, start, stop, output);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->zrevrange(key, start, stop, output);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Get a range of members by score (ordered from lowest to highest).
     *
     * Example:
     * @code{.cpp}
     *       // Send *ZRANGEBYSCORE* command without the *WITHSCORES* option:
     *       std::vector<std::string> result;
     *       // Get members whose score between (3, 6).
     *       redis.zrangebyscore("zset", BoundedInterval<double>(3, 6, BoundType::OPEN),
     *           std::back_inserter(result));
     *       // Send command with *WITHSCORES* option:
     *       std::vector<std::pair<std::string, double>> with_score;
     *       // Get members whose score between [3, +inf).
     *       redis.zrangebyscore("zset", LeftBoundedInterval<double>(3, BoundType::RIGHT_OPEN),
     *           std::back_inserter(with_score));
     * @endcode
     * @param key Key where the sorted set is stored.
     * @param interval the min-max range by score.
     * @param output Output iterator to the destination where the result is saved.
     * @note This method can also return the score of each member. If `output` is an iterator
     *       to a container of `std::string`, we send *ZRANGEBYSCORE key min max* command.
     *       If it's an iterator to a container of `std::pair<std::string, double>`,
     *       we send *ZRANGEBYSCORE key min max WITHSCORES* command. See the *Example* part on
     *       how to use this method.
     */
    template <typename Interval, typename Output>
    void zrangebyscore(const StringView &key, const Interval &interval, Output output) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->zrangebyscore(key, interval, output);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->zrangebyscore(key, interval, output);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    template <typename Interval, typename Output>
    void zrevrangebyscore(const StringView &key, const Interval &interval, Output output) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->zrevrangebyscore(key, interval, output);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->zrevrangebyscore(key, interval, output);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     * @brief Get the score of the given member.
     * @param key Key where the sorted set is stored.
     * @param member Member.
     * @return The score of the member.
     * @note If member does not exist, `zscore` returns `OptionalDouble{}` (`std::nullopt`).
     */
    OptionalDouble zscore(const StringView &key, const StringView &member);

    /**
     * @brief Add the given element to a hyperloglog.
     * @param key Key of the hyperloglog.
     * @param element Element to be added.
     * @return Whether any of hyperloglog's internal register has been altered.
     * @retval true If at least one internal register has been altered.
     * @retval false If none of internal registers has been altered.
     * @note When `pfadd` returns false, it does not mean that this method failed to add
     *       an element to the hyperloglog. Instead it means that the internal registers
     *       were not altered. If `pfadd` fails, it will throw an exception of `Exception` type.
     */
    bool pfadd(const StringView &key, const StringView &element);

    /**
     * @brief Add the given elements to a hyperloglog.
     * @param key Key of the hyperloglog.
     * @param first Iterator to the first element.
     * @param last Off-the-end iterator to the given range.
     * @return Whether any of hyperloglog's internal register has been altered.
     * @retval true If at least one internal register has been altered.
     * @retval false If none of internal registers has been altered.
     * @note When `pfadd` returns false, it does not mean that this method failed to add
     *       an element to the hyperloglog. Instead it means that the internal registers
     *       were not altered. If `pfadd` fails, it will throw an exception of `Exception` type.
     */
    template <typename Input>
    bool pfadd(const StringView &key, Input first, Input last) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->pfadd(key, first, last);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->pfadd(key, first, last);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    template <typename T>
    bool pfadd(const StringView &key, std::initializer_list<T> il) {
        return pfadd(key, il.begin(), il.end());
    }

    long long pfcount(const StringView &key);

    template <typename Input>
    long long pfcount(Input first, Input last) {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->pfcount(first, last);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->pfcount(first, last);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    template <typename T>
    long long pfcount(std::initializer_list<T> il) {
        return pfcount(il.begin(), il.end());
    }

    void pfmerge(const StringView &destination, const StringView &key);

    long long publish(const StringView &channel, const StringView &message);

    Pipeline pipeline(bool new_connection = true, const StringView &hash_tag = "");
};

}  // namespace redis
}  // namespace server
