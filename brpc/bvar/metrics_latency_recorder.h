#ifndef METRICS_LATENCY_RECORDER_H
#define METRICS_LATENCY_RECORDER_H

#include "bvar/recorder.h"
#include "bvar/reducer.h"
#include "bvar/passive_status.h"
#include "bvar/detail/percentile.h"
#include "bvar/latency_recorder.h"
#include "bvar/metrics_count_recorder.h"
#include <unordered_map>  // rehash don't change the address of value but FlatMap will

namespace bvar {
namespace detail {

typedef Window<Maxer<uint64_t>, SERIES_IN_SECOND> UMaxWindow;
// NOTE: Always use uint64_t in the interfaces no matter what the impl. is.
class MetricsLatencyBase {
public:
    explicit MetricsLatencyBase(time_t window_size);
    time_t window_size() const {
        return _max_latency_window.window_size();
    }

protected:
    // 8~9 bvar
    IntRecorder _latency;
    Maxer<uint64_t> _max_latency;
    Percentile _latency_percentile;

    UMaxWindow _max_latency_window;
    PassiveStatus<uint64_t> _count;
    PassiveStatus<uint64_t> _sum;
    // PassiveStatus<int64_t> _qps;
    PercentileWindow _latency_percentile_window;
    PassiveStatus<uint64_t> _latency_50;   // 50.0%
    PassiveStatus<uint64_t> _latency_90;   // 90.0%
    PassiveStatus<uint64_t> _latency_95;   // 95.0%
    PassiveStatus<uint64_t> _latency_99;   // 99.0%
    PassiveStatus<uint64_t> _latency_999;  // 99.9%
                                           // CDF _latency_cdf;
    // PassiveStatus<Vector<int64_t, 4> > _latency_percentiles;
};
}  // namespace detail

class MetricsLatencyItem : public detail::MetricsLatencyBase {
public:
    MetricsLatencyItem() : MetricsLatencyBase(-1) {}  // -1 use FLAGS_bvar_dump_interval
    explicit MetricsLatencyItem(time_t window_size) : MetricsLatencyBase(window_size) {}

    ~MetricsLatencyItem();

    MetricsLatencyItem& operator<<(uint64_t latency);
    void hide();

    int expose(
        const std::string&,
        const std::string&,
        const std::string&,
        const std::vector<std::pair<std::string, std::string>>&);

    int expose(const std::string&, const std::string&, const std::vector<std::pair<std::string, std::string>>&);

    // Get |ratio|-ile latency in recent |window_size| seconds
    // E.g. 0.99 means 99%-ile
    uint64_t latency_percentile(double ratio) const;
};

namespace detail {
class GlobalMetricsLatencyMap {
    typedef std::unordered_map<uint64_t, MetricsLatencyItem> LatencyItemMap;
    typedef LatencyItemMap::iterator LatencyItemMapIter;

public:
    static GlobalMetricsLatencyMap* GetInstance() {
        return Singleton<GlobalMetricsLatencyMap>::get();
    }

    GlobalMetricsLatencyMap() : g_latency_recorder_map(199) {}

    MetricsLatencyItem* GetLatencyItem(
        const uint64_t key,
        const std::string& metrics_name,
        const std::string& metrics_desc,
        const std::vector<std::pair<std::string, std::string>>& fixed_labels,
        const std::vector<std::string>& unfixed_labels_keys,
        const std::vector<std::string>& unfixed_labels_vals,
        const std::string& fixed_labels_str);

    ~GlobalMetricsLatencyMap();

private:
    LatencyItemMap g_latency_recorder_map;
    butil::RWLock g_map_mutex;
    std::unordered_set<std::string> g_desc_setting_rec;
};

class GlobalMetricsLatencyMapWithCleaning {
    typedef std::shared_ptr<MetricsLatencyItem> LatencyItemPtr;
    typedef struct {
        LatencyItemPtr item;
        int64_t last_active_time;
        std::string metrics_name;
    } MetricsLatencyItemWithTime;
    typedef std::unordered_map<uint64_t, MetricsLatencyItemWithTime> LatencyItemMap;
    typedef LatencyItemMap::iterator LatencyItemMapIter;

public:
    static GlobalMetricsLatencyMapWithCleaning* GetInstance() {
        return Singleton<GlobalMetricsLatencyMapWithCleaning>::get();
    }

    LatencyItemPtr GetLatencyItem(
        const uint64_t key,
        const std::string& metrics_name,
        const std::string& metrics_desc,
        const std::vector<std::pair<std::string, std::string>>& fixed_labels,
        const std::vector<std::string>& unfixed_labels_keys,
        const std::vector<std::string>& unfixed_labels_vals);

    GlobalMetricsLatencyMapWithCleaning() : g_latency_recorder_with_time_map(199), _tid(0), _stop_flag(false) {
        int rc = bthread_start_background(&_tid, NULL, GlobalMetricsLatencyMapWithCleaning::cleaning_task, this);
        if (rc != 0) {
            LOG(ERROR) << "start bthread for cleaning fail!!!";
            return;
        }
    }

    ~GlobalMetricsLatencyMapWithCleaning();

private:
    LatencyItemMap g_latency_recorder_with_time_map;
    butil::RWLock _mutex;
    bthread_t _tid;
    std::unordered_set<std::string> g_desc_setting_rec;
    bool _stop_flag;

    static void* cleaning_task(void*);
};
}  // namespace detail

class MetricsLatencyRecorder : public detail::MetricsRecorderBase {
    typedef std::unordered_map<uint32_t, MetricsLatencyItem> LatencyItemMap;
    typedef LatencyItemMap::iterator LatencyItemMapIter;
    typedef std::shared_ptr<MetricsLatencyItem> LatencyItemPtr;

public:
    explicit MetricsLatencyRecorder(std::string metrics_name, std::string metrics_desc) :
            MetricsRecorderBase(metrics_name, metrics_desc), _latency_recorder_map(64), _has_set_metrics_desc(false) {
        _error_latency_item_ptr = std::make_shared<MetricsLatencyItem>();
    }
    ~MetricsLatencyRecorder();
    MetricsLatencyItem& find(const std::vector<std::string>& keys);
    LatencyItemPtr find2(const std::vector<std::string>& keys);

    void hide() override;

    MetricsLatencyItem _error_latency_item;
    LatencyItemMap _latency_recorder_map;
    LatencyItemPtr _error_latency_item_ptr;

private:
    DISALLOW_COPY_AND_ASSIGN(MetricsLatencyRecorder);
    butil::RWLock _map_mutex;
    bool _has_set_metrics_desc;

    MetricsLatencyItem& find_in_global_map(const std::string& unfixed_keys, const std::vector<std::string>& keys);
};

}  // namespace bvar

#endif /* METRICS_LATENCY_RECORDER_H */
