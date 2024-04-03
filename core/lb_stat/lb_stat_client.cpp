#include "lb_stat_client.h"
#include "strategy_shm.h"
#include "butil/string_printf.h"
#include "core/common/name_agent_client.h"

using namespace server::lb_stat;
using server::common::NameAgentClient;

DEFINE_int32(report_batch_num, 1800, "real report max size");
DEFINE_int32(report_interval_ms, 200, "lb report interval");

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

static ServerStats* GetStatsByAddr(const std::string& service_name, const std::string& ip, int port) {
    std::string key = butil::string_printf("%s__%s:%d", service_name.c_str(), ip.c_str(), port);
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
        stats->ip = ip;
        stats->port = port;
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
        copy.ip = stats_item->ip;
        copy.port = stats_item->port;
        copy.succ_cnt = __sync_fetch_and_and(&stats_item->succ_cnt, 0);
        copy.fail_cnt = __sync_fetch_and_and(&stats_item->fail_cnt, 0);
        copy.fail_net_cnt = __sync_fetch_and_and(&stats_item->fail_net_cnt, 0);
        copy.fail_logic_cnt = __sync_fetch_and_and(&stats_item->fail_logic_cnt, 0);
        copy.used_ms = __sync_fetch_and_and(&stats_item->used_ms, 0);
    }
}

LbStatClient* LbStatClient::GetInstance() {
    return Singleton<LbStatClient>::get();
}

LbStatClient::LbStatClient() : _is_asked_to_stop(false) {}

void LbStatClient::Init() {
    // register lb stat
    brpc::policy::BaseLbStatExtension()->RegisterOrDie(LB_STAT_CLIENT, this);

    // start report thread
    _report_thread = std::thread(&LbStatClient::RealReport, this);
}

void LbStatClient::Stop() {
    _is_asked_to_stop = true;
    _report_thread.join();
}

bool LbStatClient::IsSvrBlock(const butil::EndPoint& endpoint) {
    return true;
}

void LbStatClient::RealReport() {
    LOG(INFO) << "Lb report thread start...";
    while (!_is_asked_to_stop) {
        DoLbReport();
        usleep(FLAGS_report_interval_ms * 1000);
    }
    LOG(INFO) << "Lb report thread finished...";
}

int LbStatClient::LbStatReport(
    const std::string& service_name,
    const butil::EndPoint& endpoint,
    int ret,
    bool responsed,
    int cost_time) {
    if (endpoint.port <= 0 || endpoint.port > 65535 || !memcmp(butil::ip2str(endpoint.ip).c_str(), "0.0.0.0", 7)) {
        return 0;
    }
    name_agent::LbStatInfo info;
    info.mutable_endpoint()->set_ip(butil::ip2str(endpoint.ip).c_str());
    info.mutable_endpoint()->set_port(endpoint.port);
    info.set_service_name(service_name);
    if ((!responsed && ret)) {
        // no response and ret!=0, take as net err or EREJECT/ELIMIT/ELOGOFF take as sys err
        info.set_fail_cnt(1);
        info.set_fail_net_cnt(1);
        LOG(INFO) << "report net err, service_name:" << service_name << ", " << butil::endpoint2str(endpoint).c_str()
                  << ", ret:" << ret << ", responsed:" << responsed << ", cost_time:" << cost_time << " ms";
    } else if (responsed && ret) {
        info.set_fail_cnt(1);
        info.set_fail_logic_cnt(1);
        LOG(INFO) << "report logic error, service_name:" << service_name << ", "
                  << butil::endpoint2str(endpoint).c_str() << ", ret:" << ret << ", cost_time:" << cost_time << " ms";
    } else {  // not report logic err yet
        info.set_succ_cnt(1);
    }
    info.set_used_ms(cost_time);

    ServerStats* stats = GetStatsByAddr(service_name, info.endpoint().ip(), info.endpoint().port());

#define _ADD_COUNTER(field) __sync_fetch_and_add(&stats->field, info.field())
    _ADD_COUNTER(succ_cnt);
    _ADD_COUNTER(fail_cnt);
    _ADD_COUNTER(fail_net_cnt);
    _ADD_COUNTER(fail_logic_cnt);
    _ADD_COUNTER(used_ms);
#undef _ADD_COUNTER
    return 0;
}

int LbStatClient::DoLbReport() {
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
            info->mutable_endpoint()->set_ip(s.ip);
            info->mutable_endpoint()->set_port(s.port);
#define _COPY_FIELD(field) info->set_##field(s.field)
            _COPY_FIELD(succ_cnt);
            _COPY_FIELD(fail_cnt);
            _COPY_FIELD(fail_net_cnt);
            _COPY_FIELD(fail_logic_cnt);
            _COPY_FIELD(used_ms);
#undef _COPY_FIELD

            LOG(DEBUG) << "real lb_report, info, service_name:" << info->service_name() << ", ip:" << s.ip
                       << ", port:" << s.port << ", succ_cnt:" << info->succ_cnt() << " , fail_cnt:" << info->fail_cnt()
                       << ", used_ms:" << info->used_ms();
        }

        if (req.infos_size() > 0) {
            NameAgentClient::GetInstance()->LbStatReport(req);
        }

        pos = idx;
    } while (idx < stats.size());

    return 0;
}