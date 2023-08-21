#pragma once

#include <brpc/controller.h>
#include <functional>

namespace server {
namespace common {

template <typename ResponseType>
class OnRPCDone : public google::protobuf::Closure {
    using Function = std::function<void(bool, ResponseType*)>;

public:
    OnRPCDone(Function func) : callback_func(func) { }

    void Run() {
        std::unique_ptr<OnRPCDone> self_guard(this);
        callback_func(cntl.Failed(), &response);
    }

    ResponseType response;
    Function callback_func;
    brpc::Controller cntl;
};

} // namespace common
} // namespace server