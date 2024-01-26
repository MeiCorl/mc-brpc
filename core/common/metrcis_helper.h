#pragma once
#include "bvar/metrics_count_recorder.h"
#include "bvar/metrics_latency_recorder.h"

///////// define
#define DEFINE_METRICS_counter_u64(name, metrics_name, desc)                         \
    namespace mc {                                                                   \
    namespace mc_metrics {                                                           \
    bvar::MetricsCountRecorder<uint64_t> METRICS_COUNTER_##name(metrics_name, desc); \
    }                                                                                \
    }                                                                                \
    using mc::mc_metrics::METRICS_COUNTER_##name
#define DEFINE_METRICS_latency(name, metrics_name, desc)                     \
    namespace mc {                                                           \
    namespace mc_metrics {                                                   \
    bvar::MetricsLatencyRecorder METRICS_LATENCY_##name(metrics_name, desc); \
    }                                                                        \
    }                                                                        \
    using mc::mc_metrics::METRICS_LATENCY_##name

///////// declare
#define DECLARE_METRICS_counter_u64(name)                               \
    namespace mc {                                                      \
    namespace mc_metrics {                                              \
    extern bvar::MetricsCountRecorder<uint64_t> METRICS_COUNTER_##name; \
    }                                                                   \
    }                                                                   \
    using mc::mc_metrics::METRICS_COUNTER_##name
#define DECLARE_METRICS_latency(name)                           \
    namespace mc {                                              \
    namespace mc_metrics {                                      \
    extern bvar::MetricsLatencyRecorder METRICS_LATENCY_##name; \
    }                                                           \
    }                                                           \
    using mc::mc_metrics::METRICS_LATENCY_##name

///////// set fixed labels
#define ADD_STATIC_counter_label(name, label_key, label_val) \
    mc::mc_metrics::METRICS_COUNTER_##name.set_metrics_label(label_key, label_val)
#define ADD_STATIC_latency_label(name, label_key, label_val) \
    mc::mc_metrics::METRICS_LATENCY_##name.set_metrics_label(label_key, label_val)

//////// set unfixed labels
#define ADD_counter_label(name, label_key) mc::mc_metrics::METRICS_COUNTER_##name.set_metrics_label(label_key)
#define ADD_latency_label(name, label_key) mc::mc_metrics::METRICS_LATENCY_##name.set_metrics_label(label_key)

//////// set metrics
#define ADD_METRICS_COUNTER(name, args...)          mc::mc_metrics::METRICS_COUNTER_##name.find({args}) << 1
#define ADD_METRICS_COUNTER_VAL(name, val, args...) mc::mc_metrics::METRICS_COUNTER_##name.find({args}) << val
#define SET_METRICS_LATENCY(name, latency, args...) mc::mc_metrics::METRICS_LATENCY_##name.find({args}) << latency
