#pragma once
#include "test.pb.h"

namespace test {
class ServiceImpl : public TestService {
public:
    ServiceImpl(/* args */);
    ~ServiceImpl();

    virtual void UpdateUserInfo(google::protobuf::RpcController* cntl_base,
                                const UpdateUserInfoReq* request,
                                UpdateUserInfoRes* response,
                                google::protobuf::Closure* done);
};

} // namespace test