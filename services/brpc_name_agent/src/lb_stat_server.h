#pragma once
#include <unordered_map>
#include "butil/memory/singleton.h"
#include "bthread/mutex.h"
#include "core/lb_stat/name_agent.pb.h"
#include "core/lb_stat/strategy_shm.h"

namespace name_agent {

using namespace server::lb_stat;
using LbStatInfoMap = std::unordered_map<std::string, ServerStats*>;

class LbStatSvr {
public:
    LbStatSvr();
    virtual ~LbStatSvr();

    static LbStatSvr* GetInstance() {
        return Singleton<LbStatSvr>::get();
    }

    int LbAddStat(const name_agent::LbStatInfo& info);

    void GenerateStrategy();

private:
    void SwapSvrStat();
    void ReleaseStatMap(LbStatInfoMap* p_stat);

    LbStatInfoMap _svr_stat_list[2];  // double buffer for in use
    int _inuse_idx;
    LbStatInfoMap* _cur_svr_stat;  // key for "ip:port"
    bthread::Mutex _mutex;         // mutex for _cur_svr_stat
};
}  // namespace name_agent