#pragma once
#include "bvar/metrics_count_recorder.h"

///////// define
#define DEFINE_METRICS_counter_u64(name, metrics_name, desc)                         \
    namespace mc {                                                                   \
    namespace mc_metrics {                                                           \
    bvar::MetricsCountRecorder<uint64_t> METRICS_COUNTER_##name(metrics_name, desc); \
    }                                                                                \
    }                                                                                \
    using mc::mc_metrics::METRICS_COUNTER_##name

///////// declare
#define DECLARE_METRICS_counter_u64(name)                               \
    namespace mc {                                                      \
    namespace mc_metrics {                                              \
    extern bvar::MetricsCountRecorder<uint64_t> METRICS_COUNTER_##name; \
    }                                                                   \
    }                                                                   \
    using mc::mc_metrics::METRICS_COUNTER_##name

///////// set fixed labels
#define ADD_STATIC_COUNTER_LABEL(name, label_key, label_val) \
    mc::mc_metrics::METRICS_COUNTER_##name.set_metrics_label(label_key, label_val)

//////// set unfixed labels
#define ADD_COUNTER_LABEL(name, label_key) mc::mc_metrics::METRICS_COUNTER_##name.set_metrics_label(label_key)

//////// set metrics
#define ADD_METRICS_COUNTER(name, args...)          mc::mc_metrics::METRICS_COUNTER_##name.find({args}) << 1
#define ADD_METRICS_COUNTER_VAL(name, val, args...) mc::mc_metrics::METRICS_COUNTER_##name.find({args}) << val
