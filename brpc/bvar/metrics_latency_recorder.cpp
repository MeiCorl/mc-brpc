#include "bvar/metrics_latency_recorder.h"
// #include "brpc/controller.h"

namespace brpc {
extern bool IsAskedToQuit();
}

namespace bvar {
extern int g_labels_max_size;
extern Adder<int64_t>* g_metricslatencyrecorder_counter;
extern bool g_enable_metrics_shard_map;
extern int64_t g_metrics_cleaning_cycle;
extern int64_t g_metrics_timeout_s;

DECLARE_int64(latency_recorder_max_num);

namespace detail {
typedef PercentileSamples<1022> CombinedPercentileSamples;

static uint64_t get_recorder_count(void* arg) {
    return static_cast<IntRecorder*>(arg)->get_value().num;
}

static uint64_t get_recorder_sum(void* arg) {
    return static_cast<IntRecorder*>(arg)->get_value().sum;
}

template <int64_t numerator, int64_t denominator>
static uint64_t get_percetile(void* arg) {
    return ((MetricsLatencyItem*)arg)->latency_percentile((double)numerator / double(denominator));
}

// Caller is responsible for deleting the return value.
static CombinedPercentileSamples* combine(PercentileWindow* w) {
    CombinedPercentileSamples* cb = new CombinedPercentileSamples;
    std::vector<GlobalPercentileSamples> buckets;
    w->get_samples(&buckets);
    cb->combine_of(buckets.begin(), buckets.end());
    return cb;
}

MetricsLatencyBase::MetricsLatencyBase(time_t window_size) :
        _max_latency(0),
        _max_latency_window(&_max_latency, window_size),
        _count(get_recorder_count, &_latency),
        _sum(get_recorder_sum, &_latency),
        _latency_percentile_window(&_latency_percentile, window_size),
        _latency_50(get_percetile<50, 100>, this),
        _latency_90(get_percetile<90, 100>, this),
        _latency_95(get_percetile<95, 100>, this),
        _latency_99(get_percetile<99, 100>, this),
        _latency_999(get_percetile<999, 1000>, this) {}

MetricsLatencyItem* GlobalMetricsLatencyMap::GetLatencyItem(
    const uint64_t key,
    const std::string& metrics_name,
    const std::string& metrics_desc,
    const std::vector<std::pair<std::string, std::string>>& fixed_labels,
    const std::vector<std::string>& unfixed_labels_keys,
    const std::vector<std::string>& unfixed_labels_vals,
    const std::string& fixed_labels_str) {
    if (key == 0) {
        LOG(ERROR) << "key is zero!";
        return NULL;
    }
    if (unfixed_labels_keys.size() != unfixed_labels_vals.size()) {
        LOG(ERROR) << "size is different!";
        return NULL;
    }
    LatencyItemMapIter iter;
    {
        butil::ReadLockGuard rg(g_map_mutex);
        iter = g_latency_recorder_map.find(key);
        if (iter != g_latency_recorder_map.end()) {
            return &(iter->second);
        }
    }
    {
        if (g_metricslatencyrecorder_counter &&
            g_metricslatencyrecorder_counter->get_value() > FLAGS_latency_recorder_max_num) {
            LOG_EVERY_N(ERROR, 10) << "create metrics_latency_recorder reached limit "
                                   << FLAGS_latency_recorder_max_num;
            return NULL;
        }
        butil::WriteLockGuard wg(g_map_mutex);
        // double check
        iter = g_latency_recorder_map.find(key);
        if (iter == g_latency_recorder_map.end()) {
            std::vector<std::pair<std::string, std::string>> labels(fixed_labels);
            size_t unfixed_labels_size = unfixed_labels_keys.size();
            for (size_t index = 0; index < unfixed_labels_size; ++index) {
                labels.push_back(std::make_pair(unfixed_labels_keys.at(index), unfixed_labels_vals.at(index)));
            }
            int rc(0);
            if (g_desc_setting_rec.find(metrics_name) == g_desc_setting_rec.end()) {
                g_desc_setting_rec.insert(metrics_name);
                rc = g_latency_recorder_map[key].expose(std::to_string(key), metrics_name, metrics_desc, labels);
            } else {
                rc = g_latency_recorder_map[key].expose(std::to_string(key), metrics_name, labels);
            }
            if (rc != 0) {
                LOG(ERROR) << "expose bvar " << key << " failed! name : " << metrics_name
                           << " val : " << fixed_labels_str;
            }
            if (g_metricslatencyrecorder_counter) {
                (*g_metricslatencyrecorder_counter) << 1;
            }
            iter = g_latency_recorder_map.find(key);
        }
        return &(iter->second);
    }
}

GlobalMetricsLatencyMap::~GlobalMetricsLatencyMap() {
    {
        butil::WriteLockGuard wg(g_map_mutex);
        if (g_metricslatencyrecorder_counter) {
            (*g_metricslatencyrecorder_counter) << -(g_latency_recorder_map.size());
        }
        for (auto& kv_item : g_latency_recorder_map) {
            kv_item.second.hide();
        }
    }
    g_desc_setting_rec.clear();
}

GlobalMetricsLatencyMapWithCleaning::LatencyItemPtr GlobalMetricsLatencyMapWithCleaning::GetLatencyItem(
    const uint64_t key,
    const std::string& metrics_name,
    const std::string& metrics_desc,
    const std::vector<std::pair<std::string, std::string>>& fixed_labels,
    const std::vector<std::string>& unfixed_labels_keys,
    const std::vector<std::string>& unfixed_labels_vals) {
    if (key == 0) {
        LOG(ERROR) << "key is zero!";
        return NULL;
    }

    if (unfixed_labels_keys.size() != unfixed_labels_vals.size()) {
        LOG(ERROR) << "unfixed labels' s size is different from unfixed vals!";
        return NULL;
    }
    LatencyItemMapIter iter;
    {
        butil::ReadLockGuard rg(_mutex);
        iter = g_latency_recorder_with_time_map.find(key);
        if (iter != g_latency_recorder_with_time_map.end()) {
            (iter->second).last_active_time = butil::cpuwide_time_s();
            return (iter->second).item;
        }
    }
    {
        if (g_metricslatencyrecorder_counter &&
            g_metricslatencyrecorder_counter->get_value() > FLAGS_latency_recorder_max_num) {
            LOG_EVERY_N(ERROR, 10) << "create metrics_latency_recorder reached limit "
                                   << FLAGS_latency_recorder_max_num;
            return NULL;
        }
        butil::WriteLockGuard wg(_mutex);
        // double check
        iter = g_latency_recorder_with_time_map.find(key);
        if (iter == g_latency_recorder_with_time_map.end()) {
            MetricsLatencyItemWithTime item_with_time;
            item_with_time.item = std::make_shared<MetricsLatencyItem>();
            item_with_time.metrics_name = metrics_name;

            std::string unfixed_labels_vals_str("");
            std::vector<std::pair<std::string, std::string>> labels(fixed_labels);
            size_t unfixed_labels_size = unfixed_labels_keys.size();
            for (size_t index = 0; index < unfixed_labels_size; ++index) {
                labels.push_back(std::make_pair(unfixed_labels_keys.at(index), unfixed_labels_vals.at(index)));
                unfixed_labels_vals_str += (unfixed_labels_keys.at(index) + ":" + unfixed_labels_vals.at(index) + " ");
            }
            int rc(0);
            if (g_desc_setting_rec.find(metrics_name) == g_desc_setting_rec.end()) {
                g_desc_setting_rec.insert(metrics_name);
                rc = (item_with_time.item)->expose(std::to_string(key), metrics_name, metrics_desc, labels);
            } else {
                rc = (item_with_time.item)->expose(std::to_string(key), metrics_name, labels);
            }
            if (rc) {
                LOG(ERROR) << "expose bvar " << key << " failed! name : " << metrics_name
                           << " val : " << unfixed_labels_vals_str;
                return NULL;
            }
            auto ins_pair = g_latency_recorder_with_time_map.insert({key, item_with_time});
            if (!ins_pair.second) {
                LOG(ERROR) << "insert into g_adder_with_time_map fail!!!";
                return NULL;
            }
            iter = ins_pair.first;
            if (g_metricslatencyrecorder_counter) {
                (*g_metricslatencyrecorder_counter) << 1;
            }
        }
        (iter->second).last_active_time = butil::cpuwide_time_s();
        return (iter->second).item;
    }
}

GlobalMetricsLatencyMapWithCleaning::~GlobalMetricsLatencyMapWithCleaning() {
    _stop_flag = true;
    if (_tid) {
        bthread_stop(_tid);
        bthread_join(_tid, NULL);
        _tid = 0;
    }
    {
        if (g_metricslatencyrecorder_counter) {
            (*g_metricslatencyrecorder_counter) << -(g_latency_recorder_with_time_map.size());
        }
        butil::WriteLockGuard wg(_mutex);
        g_latency_recorder_with_time_map.clear();
        g_desc_setting_rec.clear();
    }
}

void* GlobalMetricsLatencyMapWithCleaning::cleaning_task(void* args) {
    GlobalMetricsLatencyMapWithCleaning* gmlmwc = static_cast<GlobalMetricsLatencyMapWithCleaning*>(args);
    int64_t last_cleaning_time = 0;
    while (!brpc::IsAskedToQuit() && !(gmlmwc->_stop_flag)) {
        if (bthread_usleep(60 * 1000 * 1000) < 0) {
            if (errno == ESTOP) {
                LOG(INFO) << "Quit GlobalMetricsLatencyMapCleaning Thread=" << bthread_self();
                return NULL;
            }
            PLOG(FATAL) << "Fail to sleep";
            return NULL;
        }
        int64_t cur_time = butil::cpuwide_time_s();
        if (cur_time < last_cleaning_time + g_metrics_cleaning_cycle) {
            continue;
        }
        last_cleaning_time = cur_time;
        // rlock for seeking expired keys
        std::vector<uint64_t> expired_keys;
        {
            butil::ReadLockGuard rg(gmlmwc->_mutex);
            for (const auto& item : (gmlmwc->g_latency_recorder_with_time_map)) {
                if (cur_time - item.second.last_active_time > g_metrics_timeout_s) {
                    expired_keys.push_back(item.first);
                }
            }
        }
        if (expired_keys.empty()) {
            continue;
        }
        {
            butil::WriteLockGuard wg(gmlmwc->_mutex);
            for (const auto& key : expired_keys) {
                LatencyItemMapIter iter = (gmlmwc->g_latency_recorder_with_time_map).find(key);
                if (iter != (gmlmwc->g_latency_recorder_with_time_map).end() &&
                    cur_time - (iter->second).last_active_time > g_metrics_timeout_s) {
                    // clean up idle item
                    if ((iter->second).item.use_count() == 1) {
                        (gmlmwc->g_desc_setting_rec).erase((iter->second).metrics_name);
                        (gmlmwc->g_latency_recorder_with_time_map).erase(iter);
                        if (g_metricslatencyrecorder_counter) {
                            (*g_metricslatencyrecorder_counter) << -1;
                        }
                    }
                }
            }
        }
    }
    LOG(INFO) << "exit global metrics cleaning bthread!";
    return NULL;
}

}  // namespace detail

int MetricsLatencyItem::expose(
    const std::string& bvar_name,
    const std::string& metrics_name,
    const std::string& metrics_desc,
    const std::vector<std::pair<std::string, std::string>>& labels) {
    if (bvar_name.empty()) {
        LOG(ERROR) << "Parameter[bvar_name] is empty";
        return -1;
    }

    if (metrics_name.empty()) {
        LOG(ERROR) << "Parameter[metrics_name] is empty";
        return -1;
    }

    _latency.set_debug_name(bvar_name);
    _latency_percentile.set_debug_name(bvar_name);

#define SET_SUMMARY_VARS(var, suffix)                                                                            \
    if (var.expose_as(bvar_name, suffix, DISPLAY_METRICS_DATA) || var.set_metrics_name(metrics_name, SUMMARY)) { \
        return -1;                                                                                               \
    }                                                                                                            \
    if (!metrics_desc.empty()) {                                                                                 \
        if (var.set_metrics_desc(metrics_desc)) {                                                                \
            return -1;                                                                                           \
        }                                                                                                        \
    }                                                                                                            \
    for (const auto& label_item : labels) {                                                                      \
        var.set_metrics_label(label_item.first, label_item.second);                                              \
    }

    // suffix determines the order of printing!!!
    SET_SUMMARY_VARS(_max_latency_window, "max_latency");
    SET_SUMMARY_VARS(_sum, "na_sum");
    SET_SUMMARY_VARS(_count, "nb_count");
    SET_SUMMARY_VARS(_latency_50, "latency_50");
    SET_SUMMARY_VARS(_latency_90, "latency_90");
    SET_SUMMARY_VARS(_latency_95, "latency_95");
    SET_SUMMARY_VARS(_latency_99, "latency_99");
    SET_SUMMARY_VARS(_latency_999, "latency_999");
#undef SET_SUMMARY_VARS
    _latency_50.set_metrics_label("quantile", "0.5");
    _latency_90.set_metrics_label("quantile", "0.9");
    _latency_95.set_metrics_label("quantile", "0.95");
    _latency_99.set_metrics_label("quantile", "0.99");
    _latency_999.set_metrics_label("quantile", "0.999");
    _max_latency_window.set_metrics_label("quantile", "1");

    return 0;
}

int MetricsLatencyItem::expose(
    const std::string& bvar_name,
    const std::string& metrics_name,
    const std::vector<std::pair<std::string, std::string>>& labels) {
    return expose(bvar_name, metrics_name, "", labels);
}

uint64_t MetricsLatencyItem::latency_percentile(double ratio) const {
    std::unique_ptr<detail::CombinedPercentileSamples> cb(
        combine((detail::PercentileWindow*)&_latency_percentile_window));
    return cb->get_number(ratio);
}

MetricsLatencyItem::~MetricsLatencyItem() {
    hide();
}

void MetricsLatencyItem::hide() {
    _max_latency_window.hide();
    _sum.hide();
    _count.hide();
    _latency_50.hide();
    _latency_90.hide();
    _latency_95.hide();
    _latency_99.hide();
    _latency_999.hide();
}

MetricsLatencyItem& MetricsLatencyItem::operator<<(uint64_t latency) {
    _latency << latency;
    _max_latency << latency;
    _latency_percentile << latency;
    return *this;
}

MetricsLatencyRecorder::~MetricsLatencyRecorder() {
    if (g_metricslatencyrecorder_counter) {
        (*g_metricslatencyrecorder_counter) << -(_latency_recorder_map.size());
    }
    hide();
}

void MetricsLatencyRecorder::hide() {
    butil::WriteLockGuard wg(_map_mutex);
    for (auto& kv_item : _latency_recorder_map) {
        kv_item.second.hide();
    }
}

MetricsLatencyItem& MetricsLatencyRecorder::find_in_global_map(
    const std::string& unfixed_keys,
    const std::vector<std::string>& keys) {
    if (unfixed_keys.empty()) {
        LOG(ERROR) << "unfixed_keys is empty!";
        return _error_latency_item;
    }

    uint64_t key(0);
    {
        detail::GlobalMetricsMapKey b128key;
        butil::MurmurHash3_x64_128_Context mm_ctx;
        butil::MurmurHash3_x64_128_Init(&mm_ctx, 0);
        butil::MurmurHash3_x64_128_Update(&mm_ctx, _metrics_name.data(), _metrics_name.size());
        butil::MurmurHash3_x64_128_Update(&mm_ctx, _fixed_labels_str.data(), _fixed_labels_str.size());
        butil::MurmurHash3_x64_128_Update(&mm_ctx, unfixed_keys.data(), unfixed_keys.size());
        butil::MurmurHash3_x64_128_Final(b128key.data, &mm_ctx);
        key = b128key.data[1];
    }

    if (!brpc::IsAskedToQuit()) {
        MetricsLatencyItem* handler = detail::GlobalMetricsLatencyMap::GetInstance()->GetLatencyItem(
            key, _metrics_name, _metrics_desc, _fixed_labels, _unfixed_labels, keys, _fixed_labels_str);
        if (handler == NULL) {
            return _error_latency_item;
        }
        return *handler;
    }
    return _error_latency_item;
}

MetricsLatencyItem& MetricsLatencyRecorder::find(const std::vector<std::string>& keys) {
    if ((keys.size() != _unfixed_labels_size) || (keys.empty())) {
        LOG(ERROR) << "parameter[keys] wrong size "
                   << "metrics_name is " << _metrics_name << " need " << _unfixed_labels_size << " labels"
                   << " but given " << keys.size();
        return _error_latency_item;
    }

    auto labels_max_size = g_labels_max_size;
    char buf[labels_max_size];
    int cur_len = 0;

#define SAFE_MEMCOPY(dst, cur_len, src, size)                                                \
    do {                                                                                     \
        if ((int)size > (labels_max_size - cur_len)) {                                       \
            LOG(ERROR) << "size of splicing unfixed labels bigger than " << labels_max_size; \
            return _error_latency_item;                                                      \
        }                                                                                    \
        memcpy(dst + cur_len, src, size);                                                    \
        cur_len += (int)size;                                                                \
    } while (0);

    for (const auto& key : keys) {
        SAFE_MEMCOPY(buf, cur_len, key.data(), key.size());
    }

#undef SAFE_MEMCOPY

    if (g_enable_metrics_shard_map) {
        return find_in_global_map(std::string(buf, cur_len), keys);
    }

    uint32_t key = brpc::policy::MurmurHash32(buf, cur_len);
    LatencyItemMapIter iter;
    {
        butil::ReadLockGuard rg(_map_mutex);
        iter = _latency_recorder_map.find(key);
    }
    if (iter == _latency_recorder_map.end()) {
        if (g_metricslatencyrecorder_counter &&
            g_metricslatencyrecorder_counter->get_value() > FLAGS_latency_recorder_max_num) {
            LOG_EVERY_N(ERROR, 10) << "create metrics_latency_recorder reached limit "
                                   << FLAGS_latency_recorder_max_num;
            return _error_latency_item;
        }
        butil::WriteLockGuard wg(_map_mutex);
        // double check
        iter = _latency_recorder_map.find(key);
        if (iter == _latency_recorder_map.end()) {
            std::string bvar_name(_metrics_name);
            for (const auto& label_item : _fixed_labels) {
                bvar_name += ("_" + label_item.second);
            }
            bvar_name += std::to_string(key);
            std::vector<std::pair<std::string, std::string>> labels(_fixed_labels);
            for (size_t index = 0; index < _unfixed_labels_size; ++index) {
                labels.push_back(std::make_pair(_unfixed_labels.at(index), keys.at(index)));
            }

            if (!_has_set_metrics_desc) {
                _has_set_metrics_desc = true;
                _latency_recorder_map[key].expose(bvar_name, _metrics_name, _metrics_desc, labels);
            } else {
                _latency_recorder_map[key].expose(bvar_name, _metrics_name, labels);
            }
            if (g_metricslatencyrecorder_counter) {
                (*g_metricslatencyrecorder_counter) << 1;
            }
            iter = _latency_recorder_map.find(key);
        }
    }
    return (iter->second);
}

MetricsLatencyRecorder::LatencyItemPtr MetricsLatencyRecorder::find2(const std::vector<std::string>& keys) {
    if (keys.size() != _unfixed_labels_size) {
        LOG(ERROR) << "parameter[keys] wrong size! "
                   << "metrics_name is " << _metrics_name << " need " << _unfixed_labels_size << " labels"
                   << " but given " << keys.size();
        return _error_latency_item_ptr;
    }

    auto labels_max_size = g_labels_max_size;
    char buf[labels_max_size];
    int cur_len = 0;

#define SAFE_MEMCOPY(dst, cur_len, src, size)                                                \
    do {                                                                                     \
        if ((int)size > (labels_max_size - cur_len)) {                                       \
            LOG(ERROR) << "size of splicing unfixed labels bigger than " << labels_max_size; \
            return _error_latency_item_ptr;                                                  \
        }                                                                                    \
        memcpy(dst + cur_len, src, size);                                                    \
        cur_len += (int)size;                                                                \
    } while (0);

    for (const auto& key : keys) {
        SAFE_MEMCOPY(buf, cur_len, key.data(), key.size());
    }

#undef SAFE_MEMCOPY

    uint64_t global_key(0);
    {
        detail::GlobalMetricsMapKey b128key;
        butil::MurmurHash3_x64_128_Context mm_ctx;
        butil::MurmurHash3_x64_128_Init(&mm_ctx, 0);
        butil::MurmurHash3_x64_128_Update(&mm_ctx, _metrics_name.data(), _metrics_name.size());
        if (!_fixed_labels_str.empty()) {
            butil::MurmurHash3_x64_128_Update(&mm_ctx, _fixed_labels_str.data(), _fixed_labels_str.size());
        }
        if (cur_len) {
            butil::MurmurHash3_x64_128_Update(&mm_ctx, buf, cur_len);
        }
        butil::MurmurHash3_x64_128_Final(b128key.data, &mm_ctx);
        global_key = b128key.data[1];
    }

    if (!brpc::IsAskedToQuit()) {
        MetricsLatencyRecorder::LatencyItemPtr handler =
            detail::GlobalMetricsLatencyMapWithCleaning::GetInstance()->GetLatencyItem(
                global_key, _metrics_name, _metrics_desc, _fixed_labels, _unfixed_labels, keys);
        if (handler) {
            return handler;
        }
    }
    return _error_latency_item_ptr;
}

}  // namespace bvar
