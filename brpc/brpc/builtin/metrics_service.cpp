#include "brpc/builtin/metrics_service.h"
#include "brpc/controller.h"     // Controller
#include "brpc/server.h"         // Server
#include "brpc/closure_guard.h"  // ClosureGuard
#include "bvar/variable.h"
#include <gflags/gflags.h>
#include <sstream>
#include <utility>

namespace bvar {
DECLARE_string(channel_metrics_name);
DECLARE_string(server_metrics_name);
DECLARE_string(channel_metrics_desc);
DECLARE_string(server_metrics_desc);
DECLARE_string(call_metrics_name);
DECLARE_string(call_metrics_desc);

DECLARE_bool(enable_logic_fail_metrics);
DECLARE_bool(enable_server_metrics_with_fip);
}  // namespace bvar

namespace brpc {

void MetricsService::default_method(
    ::google::protobuf::RpcController* cntl_base,
    const ::brpc::MetricsRequest* request,
    ::brpc::MetricsResponse* response,
    ::google::protobuf::Closure* done) {
    ClosureGuard done_guard(done);
    Controller* cntl = static_cast<Controller*>(cntl_base);
    cntl->http_response().set_content_type("text/plain");

    MsgMap message;
    butil::IOBufBuilder os;
    MetricsDumper dumper(&message);

    bvar::DumpOptions opt;
    opt.dump_metrics = true;
    opt.display_filter = bvar::DISPLAY_ON_ALL_INCLUDE_METRICS;
    const int ndump = bvar::Variable::dump_exposed(&dumper, &opt);

    if (ndump < 0) {
        cntl->SetFailed("Fail to dump metrics");
        return;
    }

    MsgMap total_qps_metrics;
    std::string cli_total = bvar::FLAGS_channel_metrics_name + "_total";
    std::string svr_total = bvar::FLAGS_server_metrics_name + "_total";
    std::string call_total = bvar::FLAGS_call_metrics_name + "_total";

#define SET_TOTAL_QPS_METRICS(field, key_name)                                           \
    auto field##total_iter = message.find(key_name);                                     \
    if (field##total_iter != message.end()) {                                            \
        total_qps_metrics.insert({field##total_iter->first, field##total_iter->second}); \
        message.erase(field##total_iter);                                                \
    }

    // find c_request_total
    SET_TOTAL_QPS_METRICS(clit, cli_total);
    // find s_request_total
    SET_TOTAL_QPS_METRICS(svrt, svr_total);
    // find c_call_total
    SET_TOTAL_QPS_METRICS(callt, call_total);

#undef SET_TOTAL_QPS_METRICS

    // first: label's key; second: label's value
    using LabelPairs = std::vector<std::pair<std::string, std::string>>;
    // key1: metrics's name; key2: string of all labels; value: vector of all labels!
    std::map<std::string, std::map<std::string, LabelPairs>> item_recorder;
    for (const auto& msg_kv : total_qps_metrics) {
        if (msg_kv.second.empty()) {
            continue;
        }
        std::string metrics_desc("");
        for (const auto& item : msg_kv.second) {
            if (!item.msg.desc.empty()) {
                metrics_desc = item.msg.desc;
                break;
            }
        }
        os << "# HELP " << msg_kv.first << " " << metrics_desc << '\n'
           << "# TYPE " << msg_kv.first << " " << bvar::MetricsTypeToStr(msg_kv.second[0].msg.type) << '\n';
        for (const auto& item : msg_kv.second) {
            os << msg_kv.first;
            if (!item.msg.labels.empty()) {
                std::ostringstream oss;
                LabelPairs back_up_labels;
                os << '{';
                size_t labels_size(item.msg.labels.size()), count(0);
                for (const auto& label : item.msg.labels) {
                    os << label.first << "=\"" << label.second << "\"" << ((++count != labels_size) ? "," : "}");
                    if (label.first != "errcode" && label.first != "tinstance" && label.first != "lerr" &&
                        label.first != "ltinstance") {
                        if (!bvar::FLAGS_enable_server_metrics_with_fip && label.first == "fip") {
                            continue;
                        }
                        oss << label.first << ":" << label.second << ";";
                        back_up_labels.push_back({label.first, label.second});
                    }
                }
                item_recorder[msg_kv.first].insert({oss.str(), back_up_labels});
            }
            os << " " << item.val;
            os << '\n';
        }
    }

    std::string cli_error = bvar::FLAGS_channel_metrics_name + "_fail";
    std::string cli_to = bvar::FLAGS_channel_metrics_name + "_timeout";
    std::string svr_error = bvar::FLAGS_server_metrics_name + "_fail";

    std::string cli_logic_error = bvar::FLAGS_channel_metrics_name + "_logic_fail";
    std::string svr_logic_error = bvar::FLAGS_server_metrics_name + "_logic_fail";

    std::string call_error = bvar::FLAGS_call_metrics_name + "_fail";
    std::string call_to = bvar::FLAGS_call_metrics_name + "_timeout";

    // Handle no fail qps recorder
#define SET_PLACEHOLDER_DATA(field, key_name, desc_name, total_key)     \
    if (total_qps_metrics.find(total_key) != total_qps_metrics.end()) { \
        auto field##fail_iter = message.find(key_name);                 \
        if (field##fail_iter == message.end()) {                        \
            MsgArray placeholder_array;                                 \
            BvarMsg placeholder_msg;                                    \
            placeholder_msg.msg.desc = desc_name;                       \
            placeholder_msg.msg.type = bvar::COUNTER;                   \
            placeholder_msg.is_placeholder = true;                      \
            placeholder_array.push_back(placeholder_msg);               \
            message.insert({key_name, placeholder_array});              \
        }                                                               \
    }

    SET_PLACEHOLDER_DATA(ce, cli_error, bvar::FLAGS_channel_metrics_desc, cli_total);
    SET_PLACEHOLDER_DATA(ct, cli_to, bvar::FLAGS_channel_metrics_desc, cli_total);
    SET_PLACEHOLDER_DATA(se, svr_error, bvar::FLAGS_server_metrics_desc, svr_total);

    SET_PLACEHOLDER_DATA(cle, call_error, bvar::FLAGS_call_metrics_desc, call_total);
    SET_PLACEHOLDER_DATA(clt, call_to, bvar::FLAGS_call_metrics_desc, call_total);

    if (bvar::FLAGS_enable_logic_fail_metrics) {
        SET_PLACEHOLDER_DATA(cle, cli_logic_error, bvar::FLAGS_channel_metrics_desc, cli_total);
        SET_PLACEHOLDER_DATA(sle, svr_logic_error, bvar::FLAGS_server_metrics_desc, svr_total);
    }

#undef SET_PLACEHOLDER_DATA

    std::map<std::string, LabelPairs> back_up;

    for (const auto& msg_kv : message) {
        bool is_write_annotate = true;
        if (msg_kv.second.empty()) {
            continue;
        }
        std::string metrics_desc("");
        for (const auto& item : msg_kv.second) {
            if (!item.msg.desc.empty()) {
                metrics_desc = item.msg.desc;
                break;
            }
        }
        std::string metrics_type = bvar::MetricsTypeToStr(msg_kv.second[0].msg.type);

        back_up.clear();
        if (msg_kv.first == cli_error || msg_kv.first == cli_to || msg_kv.first == cli_logic_error) {
            back_up = item_recorder[cli_total];
        } else if (msg_kv.first == svr_error || msg_kv.first == svr_logic_error) {
            back_up = item_recorder[svr_total];
        } else if (msg_kv.first == call_error || msg_kv.first == call_to) {
            back_up = item_recorder[call_total];
        }

        bool is_summary = (msg_kv.second[0].msg.type == bvar::SUMMARY);
        bool is_histogram = (msg_kv.second[0].msg.type == bvar::HISTOGRAM);
        // fill value
        for (const auto& item : msg_kv.second) {
            if (item.is_placeholder) {
                continue;
            }
            if (is_write_annotate) {
                is_write_annotate = false;
                os << "# HELP " << msg_kv.first << " " << metrics_desc << '\n'
                   << "# TYPE " << msg_kv.first << " " << metrics_type << '\n';
            }
            os << msg_kv.first;
            if (is_summary) {
                butil::StringPiece bvar_name(item.bname);
                if (bvar_name.ends_with("na_sum")) {
                    os << "_sum";
                } else if (bvar_name.ends_with("nb_count")) {
                    os << "_count";
                }
            }
            if (is_histogram) {
                butil::StringPiece bvar_name(item.bname);
                if (bvar_name.ends_with("na_sum")) {
                    os << "_sum";
                } else if (bvar_name.ends_with("nb_count")) {
                    os << "_count";
                } else {
                    os << "_bucket";
                }
            }

            if (!item.msg.labels.empty()) {
                std::ostringstream oss;
                os << '{';
                size_t labels_size(item.msg.labels.size()), count(0);
                for (const auto& label : item.msg.labels) {
                    os << label.first << "=\"" << label.second << "\"" << ((++count != labels_size) ? "," : "}");
                    if (!back_up.empty() && label.first != "errcode" && label.first != "tinstance" &&
                        label.first != "lerr" && label.first != "ltinstance") {
                        if (!bvar::FLAGS_enable_server_metrics_with_fip && label.first == "fip") {
                            continue;
                        }
                        oss << label.first << ":" << label.second << ";";
                    }
                }
                if (!back_up.empty()) {
                    // delete the follow code to solve the problem of metrics item mutation
                    back_up.erase(oss.str());
                }
            }
            os << " " << item.val;
            os << '\n';
        }

        if (!back_up.empty()) {
            for (auto& item : back_up) {
                if (item.second.empty()) {
                    continue;
                }
                if ((msg_kv.first == cli_logic_error || msg_kv.first == svr_logic_error) &&
                    (item.first.find("proto:baidu") == std::string::npos)) {
                    continue;
                }
                if (is_write_annotate) {
                    is_write_annotate = false;
                    os << "# HELP " << msg_kv.first << " " << metrics_desc << '\n'
                       << "# TYPE " << msg_kv.first << " " << metrics_type << '\n';
                }
                os << msg_kv.first << "{";
                size_t labels_size(item.second.size()), count(0);
                for (const auto& label : item.second) {
                    os << label.first << "=\"" << label.second << "\"" << ((++count != labels_size) ? "," : "}");
                }
                os << " 0\n";
            }
        }
    }

    os.move_to(cntl->response_attachment());
    return;
}

bool MetricsDumper::dump_metrics(
    const std::string& name,
    const butil::StringPiece& description,
    const bvar::Dumper::MetricsMsg& msg) {
    MsgMapIter iter = _content->find(msg.name);
    if (iter == _content->end()) {
        // MsgArray array;
        iter = _content->insert({msg.name, MsgArray()}).first;
    }
    BvarMsg bmsg;
    bmsg.bname = name;
    bmsg.val = description.as_string();
    bmsg.msg = msg;

    (iter->second).push_back(bmsg);

    return true;
}

}  // namespace brpc
