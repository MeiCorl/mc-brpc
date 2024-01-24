#include "metrics_count_recorder.h"
#include "brpc/reloadable_flags.h"
#include <gflags/gflags.h>


namespace brpc {
extern bool IsAskedToQuit();
}

namespace bvar {

DEFINE_int32(labels_max_size, 512, "max size of splicing unfixed labels");
static bool g_labels_max_size_assignment(const char*, int32_t v) {
    if (v > 0) {
        g_labels_max_size = v;
    }
    return v > 0;
}
BRPC_VALIDATE_GFLAG(labels_max_size, g_labels_max_size_assignment);

DEFINE_string(channel_metrics_name, "c_request", "the metrics name of client qps info");
DEFINE_string(channel_metrics_desc, "qps recorder of client", "the metrics desc of client qps info");
DEFINE_string(server_metrics_name, "s_request", "the metrics name of server qps info");
DEFINE_string(server_metrics_desc, "qps recorder of server", "the metrics desc of server qps info");

DEFINE_string(server_latency_metrics_name, "s_request_cost", "the metrics name of server latency info");
DEFINE_string(server_latency_metrics_desc, "latency recorder of server(us)", "the metrics desc of server latency info");
DEFINE_string(channel_latency_metrics_name, "c_request_cost", "the metrics name of client latency info");
DEFINE_string(
    channel_latency_metrics_desc,
    "latency recorder of client(us)",
    "the metrics desc of client latency info");

DEFINE_string(server_latency_hist_metrics_name, "s_request_cost2", "the metrics name of server latency info");
DEFINE_string(
    server_latency_hist_metrics_desc,
    "latency histogram recorder of server(us)",
    "the metrics desc of server latency info");
DEFINE_string(channel_latency_hist_metrics_name, "c_request_cost2", "the metrics name of client latency info");
DEFINE_string(
    channel_latency_hist_metrics_desc,
    "latency histogram recorder of client(us)",
    "the metrics desc of client latency info");

DEFINE_string(call_metrics_name, "c_call", "the metrics name of client call info");
DEFINE_string(call_metrics_desc, "function call recorder of client", "the metrics desc of client call info");
DEFINE_string(call_latency_metrics_name, "c_call_cost", "the metrics name of client call function latency info");
DEFINE_string(
    call_latency_metrics_desc,
    "latency recorder of client call function(us)",
    "the metrics desc of client call function latency info");
DEFINE_string(call_latency_hist_metrics_name, "c_call_cost2", "the metrics name of client call latency info");
DEFINE_string(
    call_latency_hist_metrics_desc,
    "latency histogram recorder of client rpc function(us)",
    "the metrics desc of client call latency info");

DEFINE_string(
    cli_upper_bound_str,
    "",
    "the upper bound of client latency histogram statistics, default null. The unit is μs"
    "setting example: 1000, 10000, 100000, 1000000, 2000000, 5000000");
DEFINE_string(
    svr_upper_bound_str,
    "",
    "the upper bound of server latency histogram statistics, default null. The unit is μs"
    "setting example: 1000, 10000, 100000, 1000000, 2000000, 5000000");

DEFINE_bool(enable_rpc_metrics, true, "rpc metrics switch");
DEFINE_bool(enable_logic_fail_metrics, false, "rpc metrics switch for logic error");

DEFINE_int64(count_recorder_max_num, 200000, "Maximum number of entries of bvar::MetricsCountRecorder, default 20W");
static bool g_countrecorder_max_size_assignment(const char*, int64_t v) {
    if (v > 0) {
        g_countrecorder_max_size = v;
    }
    return v > 0;
}
BRPC_VALIDATE_GFLAG(count_recorder_max_num, g_countrecorder_max_size_assignment);

DEFINE_int64(
    latency_recorder_max_num,
    15000,
    "Maximum number of entries of bvar::MetricsLatencyRecorder, default 1.5W");

DEFINE_bool(enable_metrics_share_map, false, "enable metrics data use global bvar map");
static bool g_enable_metrics_shard_map_assignment(const char*, bool v) {
    g_enable_metrics_shard_map = v;
    return true;
}
BRPC_VALIDATE_GFLAG(enable_metrics_share_map, g_enable_metrics_shard_map_assignment);

DEFINE_bool(enable_channel_latency_with_tinstance, false, "channel latency with tinstance label");
DEFINE_bool(enable_server_metrics_with_fip, false, "server metrics with fip label");

DEFINE_int64(global_metrics_cleaning_cycle, 4 * 60 * 60, "cleaning cycle for GlobalMetricsMapWithCleaning");
static bool g_metrics_cleaning_cycle_assignment(const char*, int64_t v) {
    if (v > 0) {
        g_metrics_cleaning_cycle = v;
    }
    return v > 0;
}
BRPC_VALIDATE_GFLAG(global_metrics_cleaning_cycle, g_metrics_cleaning_cycle_assignment);

DEFINE_int64(global_metrics_timeout_s, 4 * 60 * 60, "cleaning cycle for GlobalMetricsMapWithCleaning");
static bool g_metrics_timeout_s_assignment(const char*, int64_t v) {
    if (v > 0) {
        g_metrics_timeout_s = v;
    }
    return v > 0;
}
BRPC_VALIDATE_GFLAG(global_metrics_timeout_s, g_metrics_timeout_s_assignment);

DEFINE_bool(enable_metrics_cleaning_mode, true, "Cleaning mode switch, default true");

Adder<int64_t>* g_metricscountrecorder_counter(new Adder<int64_t>("countrecorder_count"));
Adder<int64_t>* g_metricslatencyrecorder_counter(new Adder<int64_t>("latencyrecorder_count"));

class MetricsCounterIniter {
public:
    MetricsCounterIniter() {
#define SET_COUNTER_METRICS(tname)                                                                \
    g_metrics##tname##recorder_counter->set_metrics_name("metrics_counter", bvar::GAUGE);         \
    g_metrics##tname##recorder_counter->set_metrics_desc("Record the number of MetricsRecorder"); \
    g_metrics##tname##recorder_counter->set_metrics_label("type", #tname);

        SET_COUNTER_METRICS(count);
        SET_COUNTER_METRICS(latency);

#undef SET_COUNTER_METRICS
    }

    ~MetricsCounterIniter() {
        delete g_metricscountrecorder_counter;
        g_metricscountrecorder_counter = NULL;
        delete g_metricslatencyrecorder_counter;
        g_metricslatencyrecorder_counter = NULL;
    }
};

MetricsCounterIniter counter_initer;

int g_labels_max_size = FLAGS_labels_max_size;
int64_t g_countrecorder_max_size = FLAGS_count_recorder_max_num;
bool g_enable_metrics_shard_map = FLAGS_enable_metrics_share_map;
int64_t g_metrics_cleaning_cycle = FLAGS_global_metrics_cleaning_cycle;
int64_t g_metrics_timeout_s = FLAGS_global_metrics_timeout_s;
bool g_metrics_cleaning_mode = FLAGS_enable_metrics_cleaning_mode;
size_t g_customize_max_bucket_size = 18;  // max size of customize bucket num

int detail::MetricsRecorderBase::set_metrics_label(const std::string& key, const std::string& val) {
    auto p = std::make_pair(key, val);
    if (std::find(_fixed_labels.begin(), _fixed_labels.end(), p) == _fixed_labels.end()) {
        _fixed_labels.push_back(p);
        _fixed_labels_str += (key + ":" + val + "|");
        return 0;
    }
    LOG(WARNING) << "label{ " << key << ":" << val << "} already exists!";
    return -1;
}

int detail::MetricsRecorderBase::set_metrics_label(const std::string& key) {
    if (std::find(_unfixed_labels.begin(), _unfixed_labels.end(), key) == _unfixed_labels.end()) {
        _unfixed_labels.push_back(key);
        ++_unfixed_labels_size;
        return 0;
    }
    LOG(WARNING) << "label " << key << " already exists!";
    return -1;
}

}  // namespace bvar
