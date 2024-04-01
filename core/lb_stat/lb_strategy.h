#pragma once
#include <string>
#include <unordered_map>
#include "strategy_shm.h"
#include "brpc/extension.h"

/**
 * LbStrategy is in charge of updating strategy_shm by personalized strategy policys,
 * inherit LbStrategy to implement your own strategys.
 * @date: 2024-04-1 10:00:00
 * @author: meicorl
 */

namespace server {
namespace lb_stat {

#define STATEGY_NAME "lb_strategy"

class LbStrategy {
public:
    static void RegisterStrategy(const LbStrategy* p_strategy) {
        brpc::Extension<const LbStrategy>::instance()->RegisterOrDie(STATEGY_NAME, p_strategy);
    }

    static const LbStrategy* GetRegisteredStrategy() {
        return brpc::Extension<const LbStrategy>::instance()->Find(STATEGY_NAME);
    }

    /**
     * UpdateStrategy is in charge of doing following works:
     * 1. scan and read from cur_hash, merge changes and rewrite into backup_hashtable
     * 2. then, switch to use backup_hashtable
     * 3. reset origin cur_hashtable and clear stat_map(do not forget)
     */
    virtual int UpdateStrategy(std::unordered_map<std::string, ServerStats*>& stat_map) const = 0;
};

class DefaultLbStrategy : public LbStrategy {
public:
    int UpdateStrategy(std::unordered_map<std::string, ServerStats*>& stat_map) const override;
};

class McLbStrategy : public LbStrategy {
public:
    McLbStrategy(const char* config_path = "../conf/lb_strategy_conf.json");
    int UpdateStrategy(std::unordered_map<std::string, ServerStats*>& stat_map) const override;

private:
    std::map<uint32_t, std::map<uint32_t, uint32_t>> _strategy_config;  // {req_cnt: {pass_rate: weight_incr}}
};

}  // namespace lb_stat
}  // namespace server