#pragma once
#include "test.pb.h"
#include <random>

namespace test {
class ServiceImpl : public TestService {
private:
    std::random_device rd;
public:
    ServiceImpl(/* args */);
    ~ServiceImpl();

    virtual void UpdateUserInfo(google::protobuf::RpcController* cntl_base,
                                const UpdateUserInfoReq* request,
                                UpdateUserInfoRes* response,
                                google::protobuf::Closure* done);
    virtual void Test(google::protobuf::RpcController* cntl_base,
                      const TestReq* request,
                      TestRes* response,
                      google::protobuf::Closure* done);
};

} // namespace test