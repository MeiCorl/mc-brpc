#include "service_impl.h"

#include "base_libs/validator/validator_util.h"
#include "core/common/common_channel.h"
#include "client/agent_service.client.h"
#include <brpc/controller.h>


using server::common::SharedPtrChannel;
using server::common::SingletonChannel;
using validator::ValidatorUtil;

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

    LOG(INFO) << "[+] Req:" << request->ShortDebugString();

    auto&& [is_valid, err_msg] = ValidatorUtil::Validate(*request);
    if (!is_valid) {
        response->set_res_code(InValidParams);
        response->set_res_msg(err_msg);
        return;
    }

    name_agent::ASyncClient client("brpc_name_agent");
    name_agent::GetUpstreamInstanceReq req;
    name_agent::GetUpstreamInstanceRes res;
    req.set_seq_id(request->seq_id());
    req.set_service_name("brpc_test");
    client.GetUpstreamInstance(&req, &res,
                               [](bool is_failed, name_agent::GetUpstreamInstanceRes* _res) {
                                   if (is_failed) {
                                       LOG(ERROR) << "[!] Rpc Error!";
                                   } else {
                                       LOG(INFO) << "[+] Rpc Succ, res: " << _res->ShortDebugString();
                                   }
                               });

    LOG(INFO) << "[+] Res:" << res.ShortDebugString();
    response->set_res_msg(res.endpoint());
}
} // namespace test