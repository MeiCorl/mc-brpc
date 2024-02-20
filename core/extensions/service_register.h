#pragma once

namespace brpc {
namespace policy {

class ServiceRegister {
public:
    virtual bool RegisterService() = 0;
    virtual void UnRegisterService() = 0;
};

}  // namespace policy
}  // namespace brpc