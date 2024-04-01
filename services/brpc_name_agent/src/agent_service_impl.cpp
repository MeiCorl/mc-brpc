#include "agent_service_impl.h"
#include "brpc/controller.h"
#include "naming_service_proxy.h"
#include "lb_stat_server.h"

#include "core/lb_stat/strategy_shm.h"

using namespace name_agent;

AgentServiceImpl::AgentServiceImpl() {
    // init NameServiceProxy and LbStatSvr instances
    NameServiceProxy::GetInstance();
    LbStatSvr::GetInstance();
}

void AgentServiceImpl::GetServers(
    google::protobuf::RpcController* controller,
    const name_agent::GetServersReq* request,
    name_agent::GetServersRes* response,
    google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_seq_id(request->seq_id());
    response->set_res_code(Success);

    ResCode res_code = NameServiceProxy::GetInstance()->GetServers(
        request->service_name(),
        request->group_strategy(),
        request->group_request_code(),
        response->mutable_endpoints());
    response->set_res_code(res_code);
}

void AgentServiceImpl::LbStatReport(
    google::protobuf::RpcController* controller,
    const name_agent::LbStatReportReq* request,
    name_agent::LbStatReportRes* response,
    google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_res_code(Success);

    LOG_EVERY_N(INFO, 10) << request->ShortDebugString();
    for (int i = 0; i < request->infos_size(); ++i) {
        LbStatSvr::GetInstance()->LbAddStat(request->infos(i));
    }
}