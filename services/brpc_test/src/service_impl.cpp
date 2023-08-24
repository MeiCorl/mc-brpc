#include "service_impl.h"
#include "core/utils/validator/validator_util.h"
#include "client/test.client.h"
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

    test::SyncClient client("brpc_test");
    client.SetLbStrategy("c_murmurhash");
    client.SetRequestCode(request->seq_id());
    test::TestReq req;
    test::TestRes res;
    req.set_seq_id(request->seq_id());
    req.set_msg("hello world");
    client.Test(&req, &res);
    if (client.Failed()) {
        LOG(ERROR) << "[!] RpcError:" << client.ErrorText();
        response->set_res_code(ServerError);
        return;
    }
    response->set_res_msg("OK");
}

void ServiceImpl::Test(google::protobuf::RpcController* cntl_base,
                       const TestReq* request,
                       TestRes* response,
                       google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    LOG(INFO) << "[+] Get request from " << cntl->remote_side();

    response->set_seq_id(request->seq_id());
    response->set_res_code(Success);
    response->set_msg(request->msg());
}
} // namespace test