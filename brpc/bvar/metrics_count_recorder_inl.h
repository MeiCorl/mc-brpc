#ifndef METRICS_QPS_RECORDER_INL_H
#define METRICS_QPS_RECORDER_INL_H

namespace brpc {
extern bool IsAskedToQuit();
}

namespace bvar {

extern int g_labels_max_size;
extern Adder<int64_t>* g_metricscountrecorder_counter;
extern int64_t g_countrecorder_max_size;
extern bool g_enable_metrics_shard_map;
extern int64_t g_metrics_cleaning_cycle;
extern int64_t g_metrics_timeout_s;

namespace detail {
template <typename T>
Adder<T>* GlobalMetricsMap<T>::GetAdder(
    const uint64_t key,
    const std::string& metrics_name,
    const metrics_data_t& type,
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
    AdderMapIter iter;
    {
        butil::ReadLockGuard rg(g_map_mutex);
        iter = g_adder_map.find(key);
        if (iter != g_adder_map.end()) {
            return iter->second;
        }
    }
    {
        if (g_metricscountrecorder_counter && g_metricscountrecorder_counter->get_value() > g_countrecorder_max_size) {
            LOG_EVERY_N(ERROR, 10) << "create metrics_count_recorder reached limit " << g_countrecorder_max_size;
            return NULL;
        }
        butil::WriteLockGuard wg(g_map_mutex);
        // double check
        iter = g_adder_map.find(key);
        if (iter == g_adder_map.end()) {
            Adder<T>* temp = new Adder<T>();
            int rc = temp->expose(std::to_string(key), DISPLAY_METRICS_DATA);
            if (rc == 0) {
                temp->set_metrics_name(metrics_name, type);
                // a metrics_name only needs to be set metrics_desc once
                if (g_desc_setting_rec.find(metrics_name) == g_desc_setting_rec.end()) {
                    g_desc_setting_rec.insert(metrics_name);
                    temp->set_metrics_desc(metrics_desc);
                }
                for (const auto& label_item : fixed_labels) {
                    temp->set_metrics_label(label_item.first, label_item.second);
                }
                size_t unfixed_labels_size = unfixed_labels_keys.size();
                for (size_t index = 0; index < unfixed_labels_size; ++index) {
                    temp->set_metrics_label(unfixed_labels_keys.at(index), unfixed_labels_vals.at(index));
                }
            } else {
                LOG(ERROR) << "expose bvar " << key << " failed! name : " << metrics_name
                           << " val : " << fixed_labels_str;
                return NULL;
            }
            auto ins_pair = g_adder_map.insert({key, temp});
            if (!ins_pair.second) {
                LOG(ERROR) << "insert into g_adder_map fail!!!";
                return NULL;
            }
            iter = ins_pair.first;
            if (g_metricscountrecorder_counter) {
                (*g_metricscountrecorder_counter) << 1;
            }
        }
        return iter->second;
    }
}

template <typename T>
GlobalMetricsMap<T>::~GlobalMetricsMap() {
    {
        if (g_metricscountrecorder_counter) {
            *(g_metricscountrecorder_counter) << -(g_adder_map.size());
        }
        butil::WriteLockGuard wg(g_map_mutex);
        for (auto& kv_item : g_adder_map) {
            delete kv_item.second;
            kv_item.second = NULL;
        }
        g_adder_map.clear();
    }
    g_desc_setting_rec.clear();
}

template <typename T>
typename GlobalMetricsMapWithCleaning<T>::AdderPtr GlobalMetricsMapWithCleaning<T>::GetAdder(
    const uint64_t key,
    const std::string& metrics_name,
    const metrics_data_t& type,
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

    AdderMapIter iter;
    {
        butil::ReadLockGuard rg(_mutex);
        iter = g_adder_with_time_map.find(key);
        if (iter != g_adder_with_time_map.end()) {
            // update time
            (iter->second).last_active_time = butil::cpuwide_time_s();
            return (iter->second).adder;
        }
    }
    {
        if (g_metricscountrecorder_counter && g_metricscountrecorder_counter->get_value() > g_countrecorder_max_size) {
            LOG_EVERY_N(ERROR, 10) << "create metrics_count_recorder reached limit " << g_countrecorder_max_size;
            return NULL;
        }
        butil::WriteLockGuard wg(_mutex);
        // double check
        iter = g_adder_with_time_map.find(key);
        if (iter == g_adder_with_time_map.end()) {
            AdderWithTime adder_with_time;
            adder_with_time.adder = std::make_shared<Adder<T>>();
            adder_with_time.metrics_name = metrics_name;
            int rc(0);
            do {
                rc = (adder_with_time.adder)->expose(std::to_string(key), DISPLAY_METRICS_DATA);
                if (rc) {
                    break;
                }
                rc = (adder_with_time.adder)->set_metrics_name(metrics_name, type);
                if (rc) {
                    break;
                }
                // a metrics_name only needs to be set metrics_desc once
                if (g_desc_setting_rec.find(metrics_name) == g_desc_setting_rec.end()) {
                    g_desc_setting_rec.insert(metrics_name);
                    rc = (adder_with_time.adder)->set_metrics_desc(metrics_desc);
                    if (rc) {
                        break;
                    }
                }
                for (const auto& label_item : fixed_labels) {
                    rc += (adder_with_time.adder)->set_metrics_label(label_item.first, label_item.second);
                }
                if (rc) {
                    break;
                }
                size_t unfixed_labels_size = unfixed_labels_keys.size();
                for (size_t i = 0; i < unfixed_labels_size; ++i) {
                    rc += (adder_with_time.adder)
                              ->set_metrics_label(unfixed_labels_keys.at(i), unfixed_labels_vals.at(i));
                }
            } while (false);

            if (rc) {
                std::string unfixed_labels_vals_str("");
                for (const auto& var : unfixed_labels_vals) {
                    unfixed_labels_vals_str += (var + " ");
                }
                LOG(ERROR) << "handle bvar " << key << " failed!"
                           << " metrics name is " << metrics_name << " unfixed labels vals has "
                           << unfixed_labels_vals_str;
                return NULL;
            }
            auto ins_pair = g_adder_with_time_map.insert({key, adder_with_time});
            if (!ins_pair.second) {
                LOG(ERROR) << "insert into g_adder_with_time_map fail!!!";
                return NULL;
            }
            iter = ins_pair.first;
            if (g_metricscountrecorder_counter) {
                (*g_metricscountrecorder_counter) << 1;
            }
        }
        (iter->second).last_active_time = butil::cpuwide_time_s();
        return (iter->second).adder;
    }
}

template <typename T>
GlobalMetricsMapWithCleaning<T>::~GlobalMetricsMapWithCleaning() {
    _stop_flag = true;
    if (_tid) {
        bthread_stop(_tid);
        bthread_join(_tid, NULL);
        _tid = 0;
    }
    {
        if (g_metricscountrecorder_counter) {
            *(g_metricscountrecorder_counter) << -(g_adder_with_time_map.size());
        }
        butil::WriteLockGuard wg(_mutex);
        g_adder_with_time_map.clear();
        g_desc_setting_rec.clear();
    }
}

template <typename T>
void* GlobalMetricsMapWithCleaning<T>::cleaning_task(void* args) {
    GlobalMetricsMapWithCleaning<T>* gmmwc = static_cast<GlobalMetricsMapWithCleaning<T>*>(args);
    int64_t last_cleaning_time = 0;
    while (!brpc::IsAskedToQuit() && !(gmmwc->_stop_flag)) {
        if (bthread_usleep(60 * 1000 * 1000) < 0) {
            if (errno == ESTOP) {
                LOG(INFO) << "Quit GlobalMetricsMapCleaningThread=" << bthread_self();
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
            butil::ReadLockGuard rg(gmmwc->_mutex);
            for (const auto& item : (gmmwc->g_adder_with_time_map)) {
                if (cur_time - item.second.last_active_time > g_metrics_timeout_s) {
                    expired_keys.push_back(item.first);
                }
            }
        }
        if (expired_keys.empty()) {
            continue;
        }
        // wlock for cleaning expired keys
        {
            butil::WriteLockGuard wg(gmmwc->_mutex);
            for (const auto& key : expired_keys) {
                AdderMapIter iter = (gmmwc->g_adder_with_time_map).find(key);
                if (iter != (gmmwc->g_adder_with_time_map).end() &&
                    cur_time - (iter->second).last_active_time > g_metrics_timeout_s) {
                    // Clean up idle elements
                    if ((iter->second).adder.use_count() == 1) {
                        (gmmwc->g_desc_setting_rec).erase((iter->second).metrics_name);
                        (gmmwc->g_adder_with_time_map).erase(iter);
                        if (g_metricscountrecorder_counter) {
                            *(g_metricscountrecorder_counter) << -1;
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

template <typename T>
Adder<T>& MetricsCountRecorder<T>::find_in_global_map(
    const std::string& unfixed_keys,
    const std::vector<std::string>& keys) {
    if (unfixed_keys.empty()) {
        LOG(ERROR) << "unfixed_keys is empty!";
        return _error_count;
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
        Adder<T>* handler = detail::GlobalMetricsMap<T>::GetInstance()->GetAdder(
            key, _metrics_name, _data_type, _metrics_desc, _fixed_labels, _unfixed_labels, keys, _fixed_labels_str);
        if (handler == NULL) {
            return _error_count;
        }
        return *handler;
    }
    return _error_count;
}

template <typename T>
Adder<T>& MetricsCountRecorder<T>::find(const std::vector<std::string>& keys) {
    if ((keys.size() != _unfixed_labels_size) || (keys.empty())) {
        LOG(ERROR) << "parameter[keys] wrong size "
                   << "metrics_name is " << _metrics_name << " need " << _unfixed_labels_size << " labels"
                   << " but given " << keys.size();
        return _error_count;
    }

    auto labels_max_size = g_labels_max_size;
    char buf[labels_max_size];
    int cur_len = 0;

#define SAFE_MEMCOPY(dst, cur_len, src, size)                                                \
    do {                                                                                     \
        if ((int)size > (labels_max_size - cur_len)) {                                       \
            LOG(ERROR) << "size of splicing unfixed labels bigger than " << labels_max_size; \
            return _error_count;                                                             \
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
    AdderMapIter iter;
    {
        butil::ReadLockGuard rg(_map_mutex);
        iter = _adder_map.find(key);
    }
    if (iter == _adder_map.end()) {
        if (g_metricscountrecorder_counter && g_metricscountrecorder_counter->get_value() > g_countrecorder_max_size) {
            LOG_EVERY_N(ERROR, 10) << "create metrics_count_recorder reached limit " << g_countrecorder_max_size;
            return _error_count;
        }
        butil::WriteLockGuard wg(_map_mutex);
        // double check
        iter = _adder_map.find(key);
        if (iter == _adder_map.end()) {
            std::string adder_name(_metrics_name);
            for (const auto& label_item : _fixed_labels) {
                adder_name += ("_" + label_item.second);
            }
            adder_name += std::to_string(key);
            Adder<T>* temp = new Adder<T>();
            temp->expose(adder_name, DISPLAY_METRICS_DATA);
            // set metrics data
            temp->set_metrics_name(_metrics_name, _data_type);
            if (!_has_set_metrics_desc) {
                _has_set_metrics_desc = true;
                temp->set_metrics_desc(_metrics_desc);
            }
            for (const auto& label_item : _fixed_labels) {
                temp->set_metrics_label(label_item.first, label_item.second);
            }
            for (size_t index = 0; index < _unfixed_labels_size; ++index) {
                temp->set_metrics_label(_unfixed_labels.at(index), keys.at(index));
            }
            auto ins_pair = _adder_map.insert({key, temp});
            if (!ins_pair.second) {
                LOG_EVERY_N(ERROR, 10) << "insert into _adder_map failed " << adder_name;
                return _error_count;
            }
            iter = ins_pair.first;
            if (g_metricscountrecorder_counter) {
                (*g_metricscountrecorder_counter) << 1;
            }
        }
    }
    return *(iter->second);
}

template <typename T>
typename MetricsCountRecorder<T>::AdderPtr MetricsCountRecorder<T>::find2(const std::vector<std::string>& keys) {
    if ((keys.size() != _unfixed_labels_size)) {
        LOG(ERROR) << "parameter[keys] wrong size! "
                   << " metrics_name = " << _metrics_name << " need " << _unfixed_labels_size << " label but given "
                   << keys.size();
        return _error_count_ptr;
    }

    auto labels_max_size = g_labels_max_size;
    char buf[labels_max_size];
    int cur_len = 0;

#define SAFE_MEMCOPY(dst, cur_len, src, size)                                                \
    do {                                                                                     \
        if ((int)size > (labels_max_size - cur_len)) {                                       \
            LOG(ERROR) << "size of splicing unfixed labels bigger than " << labels_max_size; \
            return _error_count_ptr;                                                         \
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
        MetricsCountRecorder<T>::AdderPtr handler = detail::GlobalMetricsMapWithCleaning<T>::GetInstance()->GetAdder(
            global_key, _metrics_name, _data_type, _metrics_desc, _fixed_labels, _unfixed_labels, keys);
        if (handler) {
            return handler;
        }
    }

    return _error_count_ptr;
}

template <typename T>
MetricsCountRecorder<T>::~MetricsCountRecorder() {
    {
        if (g_metricscountrecorder_counter) {
            *(g_metricscountrecorder_counter) << -(_adder_map.size());
        }
        butil::WriteLockGuard wg(_map_mutex);
        for (auto& kv_item : _adder_map) {
            delete kv_item.second;  // will be hidden
            kv_item.second = NULL;
        }
    }
}

template <typename T>
void MetricsCountRecorder<T>::hide() {
    butil::WriteLockGuard wg(_map_mutex);
    for (auto& kv_item : _adder_map) {
        (kv_item.second)->hide();
    }
}
}  // namespace bvar

#endif /* METRICS_QPS_RECORDER_INL_H */
