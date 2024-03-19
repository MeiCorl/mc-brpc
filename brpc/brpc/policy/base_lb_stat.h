#pragma once

#include "brpc/extension.h"
#include "butil/endpoint.h"
#include "brpc/naming_service.h"

#define LB_STAT_CLIENT "mc_brpc_lb"

namespace brpc {
namespace policy {

class BaseLbStat {
public:
    BaseLbStat() {}
    virtual ~BaseLbStat() {}

    // check strategy, true for pass !!!!
    virtual bool IsSvrBlock(const butil::EndPoint& endpoint) = 0;

    // virtual bool IsSvrBlock(
    //     const char* ns_protocol,
    //     const char* service_name,
    //     unsigned int region_id,
    //     unsigned int group_id) const {
    //     return true;
    // }

    // /**
    //  *
    //  * @param ns_protocol naming_service name: bigo or hello (because regionid in bigo and hello daemon is not
    //  match.)
    //  * @param service_name remote service_name
    //  * @param node service_node details: groupid, regionid, serverid, ip list
    //  * @return true for pass!!!
    //  */
    // virtual bool IsSvrBlock(const char* ns_protocol, const char* service_name, const ServerDetailNode& node) {
    // 	return true;
    // }

    // do rpc call report, responded mean if from rpc response or other timeout err
    virtual int LbStatReport(
        const std::string& service_name,
        const butil::EndPoint& endpoint,
        int ret,
        bool responsed,
        int cost_time) = 0;
};

inline brpc::Extension<BaseLbStat>* BaseLbStatExtension() {
    return brpc::Extension<BaseLbStat>::instance();
}

}  // namespace policy
}  // namespace brpc
