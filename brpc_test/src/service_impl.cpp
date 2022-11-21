#include "service_impl.h"
#include "validator/validator_util.h"
#include <brpc/controller.h>

using validator::ValidatorUtil;

namespace test {
ServiceImpl::ServiceImpl(/* args */) { }

ServiceImpl::~ServiceImpl() { }

void ServiceImpl::UpdateUserInfo(google::protobuf::RpcController* cntl_base,
                                 const UpdateUserInfoReq* request,
                                 UpdateUserInfoRes* response,
                                 google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    // brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    response->set_seq_id(request->seq_id());
    
    auto&& [is_valid, err_msg] = ValidatorUtil::Validate(*request);
    if (!is_valid) {
        response->set_res_code(InValidParams);
        response->set_res_msg(err_msg);
        return;
    }

    response->set_res_code(Success);
}
} // namespace test