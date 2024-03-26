#include "agent_service_impl.h"
#include "brpc/controller.h"

using namespace name_agent;

AgentServiceImpl::AgentServiceImpl() {
    m_pNamingServiceProxy = std::make_shared<NameServiceProxy>();
}

void AgentServiceImpl::GetServers(
    google::protobuf::RpcController* controller,
    const name_agent::GetServersReq* request,
    name_agent::GetServersRes* response,
    google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_seq_id(request->seq_id());
    response->set_res_code(Success);
    ResCode res_code = m_pNamingServiceProxy->GetServers(
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

    brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
    LOG(INFO) << "from_svr:" << cntl->from_svr_name() << " Req:" << request->ShortDebugString();
    // todo: fix me
}
