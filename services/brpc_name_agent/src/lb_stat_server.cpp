#include "lb_stat_server.h"
#include "core/lb_stat/lb_strategy.h"
#include "butil/string_printf.h"
#include "butil/time.h"
#include <thread>
#include <gflags/gflags.h>

using namespace name_agent;

#define ADD_FILEDS(stat, field) __sync_fetch_and_add(&(stat->field), info.field())
DEFINE_uint32(strategy_generate_interval_ms, 1000, "strategy generate interval");

void LbStrategyThread() {
    // init strategy shm as svr
    int ret = StrategyShm::GetInstance()->Init(0);
    if (ret) {
        LOG(ERROR) << "StrategyShm::GetInstance()->Init as svr fail, ret:" << ret;
        exit(-1);
    }

// register lb strategy
#ifdef USE_MC_STRATEGY_GENERATOR
    StrategyGenerator::RegisterStrategy(new McStrategyGenerator());
    LOG(INFO) << "using McStrategyGenerator...";
#else
    StrategyGenerator::RegisterStrategy(new DefaultStrategyGenerator());
    LOG(INFO) << "using DefaultStrategyGenerator...";
#endif

    // loop, generate lb startegy periodically
    LOG(INFO) << "lb strategy thread start...";
    while (1) {
        LbStatSvr::GetInstance()->GenerateStrategy();
        usleep(FLAGS_strategy_generate_interval_ms * 1000);
    }
}

LbStatSvr::LbStatSvr() : _inuse_idx(0) {
    _cur_svr_stat = &_svr_stat_list[_inuse_idx];

    // start strategy thread
    std::thread(&LbStrategyThread).detach();
}

LbStatSvr::~LbStatSvr() {
    BAIDU_SCOPED_LOCK(_mutex);
    ReleaseStatMap(_cur_svr_stat);
}

int LbStatSvr::LbAddStat(const name_agent::LbStatInfo& info) {
    std::string key = butil::string_printf("%s:%d", info.endpoint().ip().c_str(), info.endpoint().port());

    ServerStats* p_stat = nullptr;
    {
        BAIDU_SCOPED_LOCK(_mutex);
        auto it = _cur_svr_stat->find(key);
        if (it == _cur_svr_stat->end()) {
            p_stat = new ServerStats;
            p_stat->ip = info.endpoint().ip();
            p_stat->port = info.endpoint().port();
            p_stat->service_name.assign(info.service_name());
            (*_cur_svr_stat)[key] = p_stat;
        } else {
            p_stat = it->second;
        }
    }

    ADD_FILEDS(p_stat, succ_cnt);
    ADD_FILEDS(p_stat, fail_cnt);
    ADD_FILEDS(p_stat, fail_net_cnt);
    ADD_FILEDS(p_stat, fail_logic_cnt);
    ADD_FILEDS(p_stat, used_ms);

    return 0;
}

void LbStatSvr::SwapSvrStat() {
    BAIDU_SCOPED_LOCK(_mutex);
    ++_inuse_idx;
    _inuse_idx = _inuse_idx % 2;
    _cur_svr_stat = &_svr_stat_list[_inuse_idx];
}

void LbStatSvr::ReleaseStatMap(LbStatInfoMap* p_stat) {
    for (auto it = p_stat->begin(); it != p_stat->end();) {
        if (it->second) {
            delete it->second;
            it->second = nullptr;
        }
    }
    p_stat->clear();
}

void LbStatSvr::GenerateStrategy() {
    LbStatInfoMap* cur_stat = _cur_svr_stat;
    uint32_t size = cur_stat->size();
    SwapSvrStat();
    usleep(100 * 1000);  // wait a moment, so that all clients have switch to new LbStatInfoMap

    butil::Timer timer(butil::Timer::STARTED);

    const StrategyGenerator* p_strategy = StrategyGenerator::GetRegisteredStrategy();
    p_strategy->UpdateStrategy(*cur_stat);

    timer.stop();
    uint32_t cost_ms = timer.m_elapsed();
    LOG(INFO) << "lb strategy updated, cost_ms:" << cost_ms << ", size:" << size;
    if (cost_ms > FLAGS_strategy_generate_interval_ms - 200) {
        // TODO: alarm，cost too much time, optimization needed.
    }
}