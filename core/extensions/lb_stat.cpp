#include "lb_stat.h"
#include "butil/string_printf.h"
#include "core/common/name_agent_client.h"
#include <thread>

using namespace brpc::policy;
using server::common::NameAgentClient;

DEFINE_int32(report_batch_num, 1800, "real report max size");
DEFINE_int32(report_interval, 200, "lb report interval");

void RealReport() {
    LOG(INFO) << "Lb report thread start...";
    while (true) {
        LbStat::GetInstance()->DoLbReport();
        usleep(FLAGS_report_interval * 1000);
    }
}

struct ServerStats {
    uint32_t succ_cnt;
    uint32_t fail_cnt;
    uint32_t fail_net_cnt;
    uint32_t fail_logic_cnt;
    uint32_t used_ms;

    std::string service_name;
    std::string endpoint;

    ServerStats() : succ_cnt(0), fail_cnt(0), fail_net_cnt(0), fail_logic_cnt(0), used_ms(0) {}
};

struct GlobalStats {
    std::mutex mutex;
    std::unordered_map<std::string, ServerStats> key2stats;

    static GlobalStats* GetInstance() {
        return Singleton<GlobalStats>::get();
    }
};

struct LocalStats {
    typedef std::unordered_map<std::string, ServerStats*> LocalMap;
    LocalMap key2stats;

    LocalStats() {}
};

// thread local stats
static __thread LocalStats* t_pstats;

static LocalStats* GetLocalStats() {
    if (t_pstats != NULL) {
        return t_pstats;
    }

    LocalStats* p = new LocalStats();
    t_pstats = p;
    return p;
}

static ServerStats* GetStatsByAddr(const std::string& service_name, const std::string& endpoint) {
    std::string key = butil::string_printf("%s_%s", service_name.c_str(), endpoint.c_str());
    LocalStats* lstats = GetLocalStats();
    LocalStats::LocalMap::const_iterator it = lstats->key2stats.find(key);
    if (it != lstats->key2stats.end()) {
        return it->second;
    }

    ServerStats* stats = NULL;
    {
        GlobalStats* g_stats = GlobalStats::GetInstance();
        std::lock_guard<std::mutex> lk(g_stats->mutex);
        stats = &g_stats->key2stats[key];
        stats->endpoint = endpoint;
        stats->service_name = service_name;
    }
    lstats->key2stats[key] = stats;
    return stats;
}

static void CollectStats(std::vector<ServerStats>& stats) {
    GlobalStats* g_stats = GlobalStats::GetInstance();
    std::lock_guard<std::mutex> lk(g_stats->mutex);

    stats.reserve(g_stats->key2stats.size());

    std::unordered_map<std::string, ServerStats>::iterator it = g_stats->key2stats.begin();
    for (; it != g_stats->key2stats.end(); ++it) {
        ServerStats* stats_item = &it->second;
        if (stats_item->succ_cnt + stats_item->fail_cnt + stats_item->fail_net_cnt + stats_item->fail_logic_cnt == 0) {
            continue;
        }
        stats.push_back(ServerStats());
        ServerStats& copy = stats.back();

        copy.service_name = stats_item->service_name;
        copy.endpoint = stats_item->endpoint;
        copy.succ_cnt = __sync_fetch_and_and(&stats_item->succ_cnt, 0);
        copy.fail_cnt = __sync_fetch_and_and(&stats_item->fail_cnt, 0);
        copy.fail_net_cnt = __sync_fetch_and_and(&stats_item->fail_net_cnt, 0);
        copy.fail_logic_cnt = __sync_fetch_and_and(&stats_item->fail_logic_cnt, 0);
        copy.used_ms = __sync_fetch_and_and(&stats_item->used_ms, 0);
    }
}

LbStat* LbStat::GetInstance() {
    return Singleton<LbStat>::get();
}

LbStat::LbStat() : _last_report_timems(0), _report_interval(200) {}

LbStat::~LbStat() {}

void LbStat::Init() {
    // register lb stat
    BaseLbStatExtension()->RegisterOrDie(LB_STAT_CLIENT, this);

    // start report thread
    std::thread(&RealReport).detach();
}

bool LbStat::IsSvrBlock(const butil::EndPoint& endpoint) {
    return true;
}

int LbStat::LbStatReport(
    const std::string& service_name,
    const butil::EndPoint& endpoint,
    int ret,
    bool responsed,
    int cost_time) {
    name_agent::LbStatInfo info;
    info.set_endpoint(butil::endpoint2str(endpoint).c_str());
    info.set_service_name(service_name);
    if ((!responsed && ret)) {
        // no response and ret!=0, take as net err or EREJECT/ELIMIT/ELOGOFF take as sys err
        info.set_fail_cnt(1);
        info.set_fail_net_cnt(1);
        LOG(INFO) << "report net err, service_name:" << service_name << ", " << info.endpoint() << ", ret:" << ret
                  << ", responsed:" << responsed << ", cost_time:" << cost_time << " ms";
    } else if (responsed && ret) {
        info.set_fail_cnt(1);
        info.set_fail_logic_cnt(1);
        LOG(INFO) << "report logic error, service_name:" << service_name << ", " << info.endpoint() << ", ret:" << ret
                  << ", cost_time:" << cost_time << " ms";
    } else {  // not report logic err yet
        info.set_succ_cnt(1);
    }
    info.set_used_ms(cost_time);

    ServerStats* stats = GetStatsByAddr(service_name, info.endpoint());

#define _ADD_COUNTER(field) __sync_fetch_and_add(&stats->field, info.field())
    _ADD_COUNTER(succ_cnt);
    _ADD_COUNTER(fail_cnt);
    _ADD_COUNTER(fail_net_cnt);
    _ADD_COUNTER(fail_logic_cnt);
    _ADD_COUNTER(used_ms);
#undef _ADD_COUNTER
    return 0;
}

int LbStat::DoLbReport() {
    unsigned long now = butil::gettimeofday_ms();
    unsigned long tmp_last_report_timems = _last_report_timems.load(butil::memory_order_consume);
    if (now < tmp_last_report_timems + _report_interval) {
        // avoid report too frequent
        return 0;
    }

    // lock free cas for report control, only one thread do report
    // as confilct happens with small probability, here we can use cpu full barrel
    bool self_report = _last_report_timems.compare_exchange_strong(
        tmp_last_report_timems, now, butil::memory_order_acq_rel, butil::memory_order_acq_rel);
    if (!self_report) {
        return 0;
    }

    std::vector<ServerStats> stats;
    CollectStats(stats);

    size_t idx(0), pos(0), round(10);
    do {
        if (round == 0) {
            LOG(WARNING) << "[*] reach the limit.";
            break;
        }

        --round;
        name_agent::LbStatReportReq req;
        for (; idx < stats.size() && idx < pos + FLAGS_report_batch_num; ++idx) {
            const ServerStats& s = stats[idx];

            name_agent::LbStatInfo* info = req.add_infos();
            info->set_service_name(s.service_name);
            info->set_endpoint(s.endpoint);
#define _COPY_FIELD(field) info->set_##field(s.field)
            _COPY_FIELD(succ_cnt);
            _COPY_FIELD(fail_cnt);
            _COPY_FIELD(fail_net_cnt);
            _COPY_FIELD(fail_logic_cnt);
            _COPY_FIELD(used_ms);
#undef _COPY_FIELD

            LOG(DEBUG) << "real lb_report, info, service_name:" << info->service_name() << ", " << info->endpoint()
                      << ", succ_cnt:" << info->succ_cnt() << " , fail_cnt:" << info->fail_cnt()
                      << ", used_ms:" << info->used_ms();
        }

        if (req.infos_size() > 0) {
            NameAgentClient::GetInstance()->LbStatReport(req);
        }

        pos = idx;
    } while (idx < stats.size());

    return 0;
}