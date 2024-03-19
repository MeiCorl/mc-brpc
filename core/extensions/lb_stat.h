#pragma once

#include "brpc/policy/base_lb_stat.h"

namespace brpc {
namespace policy {

class LbStat : public BaseLbStat {
public:
    static LbStat* GetInstance();

    LbStat();
    virtual ~LbStat();

    void Init();

    virtual bool IsSvrBlock(const butil::EndPoint& endpoint) override;
    virtual int LbStatReport(
        const std::string& service_name,
        const butil::EndPoint& endpoint,
        int ret,
        bool responsed,
        int cost_time) override;

    int DoLbReport();

private:
    butil::atomic<unsigned long> _last_report_timems;  // last report time by ms
    unsigned long _report_interval;                    // interval time by ms,default 200ms
};

}  // namespace policy
}  // namespace brpc
