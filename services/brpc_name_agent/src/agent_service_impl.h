#pragma once
#include "naming_service_proxy.h"

namespace name_agent {

class AgentServiceImpl : public AgentService {
private:
    std::shared_ptr<NameServiceProxy> m_pNamingServiceProxy;

public:
    AgentServiceImpl();

    virtual void GetServers(
        google::protobuf::RpcController* controller,
        const name_agent::GetServersReq* request,
        name_agent::GetServersRes* response,
        google::protobuf::Closure* done);
    virtual void LbStatReport(
        google::protobuf::RpcController* controller,
        const name_agent::LbStatReportReq* request,
        name_agent::LbStatReportRes* response,
        google::protobuf::Closure* done);
};
}  // namespace name_agent