#include "service_impl.h"
#include "core/utils/validator/validator_util.h"
#include "client/test.client.h"
#include "brpc/server.h"
#include "core/mysql/db_manager.h"

using namespace server::utils;
namespace test {
ServiceImpl::ServiceImpl(/* args */) {}

ServiceImpl::~ServiceImpl() {}

void ServiceImpl::UpdateUserInfo(
    google::protobuf::RpcController* cntl_base,
    const UpdateUserInfoReq* request,
    UpdateUserInfoRes* response,
    google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_seq_id(request->seq_id());
    response->set_res_code(Success);
    response->set_res_msg("OK");
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);

    LOG(INFO) << "Req:{" << request->ShortDebugString() << "}, from:" << cntl->from_svr_name();
    auto&& [is_valid, err_msg] = ValidatorUtil::Validate(*request);
    if (!is_valid) {
        response->set_res_code(InValidParams);
        response->set_res_msg(err_msg);
        return;
    }

    test::SyncClient client("brpc_test");
    client.SetConnectTimeoutMs(500);
    client.SetTimeoutMs(1000);
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
}

void ServiceImpl::Test(
    google::protobuf::RpcController* cntl_base,
    const TestReq* request,
    TestRes* response,
    google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);

    auto conn = DBManager::get()->GetDBConnection("db_master");
    if(conn == nullptr) {
        response->set_seq_id(request->seq_id());
        response->set_res_code(ServerError);
        response->set_msg(request->msg());
        return;
    }

    std::string sql = "SELECT * FROM tbl_user_info limit 10";
    try {
        if (conn->query(sql)) {
            while (conn->next()) {
                std::ostringstream os;
                for (unsigned i = 0; i < conn->cols(); ++i) {
                    os << conn->value(i) << " ";
                }
                LOG(INFO) << os.str();
            }
        } else {
            LOG(ERROR) << "[!] db query error, sql:" << sql;
        }
    } catch(std::exception& e) {
        LOG(ERROR) << e.what();
    }

    response->set_seq_id(request->seq_id());
    response->set_res_code(Success);
    response->set_msg(request->msg());
}
}  // namespace test