#include "service_impl.h"
#include "core/utils/validator/validator_util.h"
#include "client/name_agent.client.h"
#include <brpc/controller.h>

using namespace server::utils;

namespace test {
ServiceImpl::ServiceImpl(/* args */) { }

ServiceImpl::~ServiceImpl() { }

void ServiceImpl::UpdateUserInfo(google::protobuf::RpcController* cntl_base,
                                 const UpdateUserInfoReq* request,
                                 UpdateUserInfoRes* response,
                                 google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_seq_id(request->seq_id());
    response->set_res_code(Success);

    auto&& [is_valid, err_msg] = ValidatorUtil::Validate(*request);
    if (!is_valid) {
        response->set_res_code(InValidParams);
        response->set_res_msg(err_msg);
        return;
    }

    name_agent::SyncClient client("brpc_name_agent");
    name_agent::GetServersReq req;
    name_agent::GetServersRes res;
    req.set_seq_id(request->seq_id());
    req.set_service_name("brpc_test");
    client.GetServers(&req, &res);
    LOG(INFO) << "[+] Res:" << res.ShortDebugString() << ", latency_us:" << client.latency_us();
    response->set_res_msg("OK");
}
} // namespace test