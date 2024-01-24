#ifndef METRICS_QPS_RECORDER_H
#define METRICS_QPS_RECORDER_H

#include "bvar/reducer.h"
#include "bvar/recorder.h"
#include <unordered_map>  // rehash don't change the address of value but FlatMap will
#include <unordered_set>
#include "bvar/utils/lock_timer.h"
#include <vector>   // std::vector
#include <utility>  // std::pair
#include <string>   // std::string
#include <algorithm>
#include <memory>
#include "brpc/policy/hasher.h"
#include "butil/third_party/murmurhash3/murmurhash3.h"
#include "butil/memory/singleton.h"
#include "bthread/bthread.h"
#include "butil/synchronization/lock.h"

namespace bvar {
namespace detail {

struct GlobalMetricsMapKey {
    uint64_t data[2];

    GlobalMetricsMapKey() {
        Reset();
    }
    void Reset() {
        data[0] = data[1] = 0;
    }
};

template <typename T>
class GlobalMetricsMap {
    typedef std::unordered_map<uint64_t, Adder<T>*> AdderMap;
    typedef typename AdderMap::iterator AdderMapIter;

public:
    static GlobalMetricsMap<T>* GetInstance() {
        return Singleton<GlobalMetricsMap<T>>::get();
    }

    Adder<T>* GetAdder(
        const uint64_t key,
        const std::string& metrics_name,
        const metrics_data_t& type,
        const std::string& metrics_desc,
        const std::vector<std::pair<std::string, std::string>>& fixed_labels,
        const std::vector<std::string>& unfixed_labels_keys,
        const std::vector<std::string>& unfixed_labels_vals,
        const std::string& fixed_labels_str);

    GlobalMetricsMap() : g_adder_map(199) {}

    ~GlobalMetricsMap();

private:
    AdderMap g_adder_map;
    butil::RWLock g_map_mutex;
    std::unordered_set<std::string> g_desc_setting_rec;
};

template <typename T>
class GlobalMetricsMapWithCleaning {
    typedef std::shared_ptr<Adder<T>> AdderPtr;
    typedef struct {
        AdderPtr adder;
        int64_t last_active_time;
        std::string metrics_name;
    } AdderWithTime;
    typedef std::unordered_map<uint64_t, AdderWithTime> AdderMap;
    typedef typename AdderMap::iterator AdderMapIter;

public:
    static GlobalMetricsMapWithCleaning<T>* GetInstance() {
        return Singleton<GlobalMetricsMapWithCleaning<T>>::get();
    }

    AdderPtr GetAdder(
        const uint64_t key,
        const std::string& metrics_name,
        const metrics_data_t& type,
        const std::string& metrics_desc,
        const std::vector<std::pair<std::string, std::string>>& fixed_labels,
        const std::vector<std::string>& unfixed_labels_keys,
        const std::vector<std::string>& unfixed_labels_vals);

    GlobalMetricsMapWithCleaning() : g_adder_with_time_map(199), _tid(0), _stop_flag(false) {
        int rc = bthread_start_background(&_tid, NULL, GlobalMetricsMapWithCleaning::cleaning_task, this);
        if (rc != 0) {
            LOG(ERROR) << "start bthread for cleaning fail!!!";
            return;
        }
    }

    ~GlobalMetricsMapWithCleaning();

private:
    static void* cleaning_task(void*);

    AdderMap g_adder_with_time_map;
    butil::RWLock _mutex;
    bthread_t _tid;
    std::unordered_set<std::string> g_desc_setting_rec;
    bool _stop_flag;
};

class MetricsRecorderBase {
public:
    explicit MetricsRecorderBase(std::string metrics_name, std::string metrics_desc) :
            _metrics_name(metrics_name), _metrics_desc(metrics_desc), _unfixed_labels_size(0), _fixed_labels_str("") {
        _fixed_labels.clear();
        _unfixed_labels.clear();
    }
    virtual ~MetricsRecorderBase() {
        _fixed_labels.clear();
        _unfixed_labels.clear();
    }

    int set_metrics_label(const std::string&, const std::string&);
    int set_metrics_label(const std::string&);
    size_t get_unfixed_labels_size() {
        return _unfixed_labels_size;
    }
    virtual void hide() = 0;

protected:
    std::string _metrics_name;
    std::string _metrics_desc;
    std::vector<std::pair<std::string, std::string>> _fixed_labels;
    std::vector<std::string> _unfixed_labels;
    size_t _unfixed_labels_size;

    std::string _fixed_labels_str;
};
}  // namespace detail

template <typename T>
class MetricsCountRecorder : public detail::MetricsRecorderBase {
    typedef std::unordered_map<uint32_t, Adder<T>*> AdderMap;
    typedef typename AdderMap::iterator AdderMapIter;
    typedef detail::MetricsRecorderBase Base;
    typedef std::shared_ptr<Adder<T>> AdderPtr;

public:
    explicit MetricsCountRecorder(
        std::string metrics_name,
        std::string metrics_desc,
        metrics_data_t data_type = GAUGE) :
            Base(metrics_name, metrics_desc), _adder_map(64), _data_type(data_type), _has_set_metrics_desc(false) {
        _error_count_ptr = std::make_shared<Adder<T>>();
    }

    ~MetricsCountRecorder();

    Adder<T>& find(const std::vector<std::string>& keys);
    // bvar::Adder<T>& operator<<(typename butil::add_cr_non_integral<T>::type value);
    AdderPtr find2(const std::vector<std::string>& keys);

    void hide() override;

    Adder<T> _error_count;  // don't expose
    AdderMap _adder_map;
    AdderPtr _error_count_ptr;  // don't expose

private:
    DISALLOW_COPY_AND_ASSIGN(MetricsCountRecorder);
    // bvar::LatencyRecorder _mutex_recorder;
    // bvar::MutexWithLatencyRecorder<pthread_mutex_t> _mutex;
    metrics_data_t _data_type;
    butil::RWLock _map_mutex;
    bool _has_set_metrics_desc;

    Adder<T>& find_in_global_map(const std::string& unfixed_keys, const std::vector<std::string>& keys);
};

}  // namespace bvar
#include "bvar/metrics_count_recorder_inl.h"

#endif /* METRICS_QPS_RECORDER_H */
