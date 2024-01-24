#ifndef METRICS_SERVICE_H
#define METRICS_SERVICE_H

#include "brpc/builtin_service.pb.h"
#include <vector>
#include <string>
#include "bvar/bvar.h"
#include "butil/iobuf.h"
#include <map>

namespace brpc {

struct BvarMsg {
    std::string bname;
    std::string val;
    bvar::Dumper::MetricsMsg msg;
    bool is_placeholder = false;
};
typedef std::vector<BvarMsg> MsgArray;
typedef std::map<std::string, MsgArray> MsgMap;
typedef MsgMap::iterator MsgMapIter;

class MetricsService : public metrics {
public:
    void default_method(
        ::google::protobuf::RpcController* cntl_base,
        const ::brpc::MetricsRequest* request,
        ::brpc::MetricsResponse* response,
        ::google::protobuf::Closure* done) override;
};

class MetricsDumper : public bvar::Dumper {
public:
    explicit MetricsDumper(MsgMap* content) : _content(content) {}
    bool dump(const std::string& name, const butil::StringPiece& desc) override {
        return true;
    }
    bool dump_metrics(
        const std::string& name,
        const butil::StringPiece& description,
        const bvar::Dumper::MetricsMsg& msg) override;

private:
    DISALLOW_COPY_AND_ASSIGN(MetricsDumper);
    MsgMap* _content;
};

}  // namespace brpc

#endif  // METRICS_SERVICE_H
