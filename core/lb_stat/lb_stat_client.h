#pragma once

#include "brpc/policy/base_lb_stat.h"
#include <thread>

namespace server {
namespace lb_stat {

class LbStatClient : public brpc::policy::BaseLbStat {
public:
    static LbStatClient* GetInstance();

    LbStatClient();

    void Init();
    void Stop();

    virtual bool IsSvrBlock(const butil::EndPoint& endpoint) override;
    virtual int LbStatReport(
        const std::string& service_name,
        const butil::EndPoint& endpoint,
        int ret,
        bool responsed,
        int cost_time) override;

private:
    int DoLbReport();
    void RealReport();

    std::thread _report_thread;
    volatile bool _is_asked_to_stop;
};

}  // namespace server
}  // namespace lb_stat
