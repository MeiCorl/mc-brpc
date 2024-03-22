#pragma once

#include "brpc/policy/base_lb_stat.h"
#include <thread>

namespace brpc {
namespace policy {

class LbStat : public BaseLbStat {
public:
    static LbStat* GetInstance();

    LbStat();

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
    butil::atomic<unsigned long> _last_report_timems;  // last report time by ms
    unsigned long _report_interval;                    // interval time by ms,default 200ms
};

}  // namespace policy
}  // namespace brpc
