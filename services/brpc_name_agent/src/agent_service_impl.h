#pragma once

#include <vector>
#include <unordered_map>
#include <functional>
#include <etcd/Watcher.hpp>
#include <butil/containers/doubly_buffered_data.h>
#include "agent_service.pb.h"
#include "core/config/server_config.h"

namespace name_agent {

using server::config::InstanceInfo;
using server::config::ServerConfig;
using std::shared_ptr;
using std::string;
using std::unordered_map;
using std::vector;

using ServiceMap       = unordered_map<string, vector<string>>;
using ServiceRegionMap = unordered_map<string, unordered_map<uint32_t, vector<string>>>;
using ServiceRegionAndGroupMap =
    unordered_map<string, unordered_map<uint32_t, unordered_map<uint32_t, vector<string>>>>;

class AgentServiceImpl : public AgentService {
private:
    butil::DoublyBufferedData<ServiceMap> m_instances; // service_name --> instances(ip:port)
    butil::DoublyBufferedData<ServiceRegionMap> m_instancesByRegion; // service_name, region_id --> instances(ip:port)
    butil::DoublyBufferedData<ServiceRegionAndGroupMap>
        m_instancesByRegionAndGroup; // service_name, region_id, group_id --> instances(ip:port)


    shared_ptr<etcd::Watcher> m_pEtcdWatcher;

    void InitializeWatcher(const string& endpoints,
                           const string& prefix,
                           std::function<void(etcd::Response)> callback,
                           shared_ptr<etcd::Watcher>& watcher);
    void InitServers();
    void AddServer(const string& service_name, const InstanceInfo& info);
    void RemoveServer(const string& service_name, const InstanceInfo& info);

public:
    AgentServiceImpl(/* args */);
    ~AgentServiceImpl();

    virtual void Test(google::protobuf::RpcController* controller,
                      const name_agent::TestReq* request,
                      name_agent::TestRes* response,
                      google::protobuf::Closure* done);
    virtual void GetUpstreamInstance(google::protobuf::RpcController* controller,
                                     const name_agent::GetUpstreamInstanceReq* request,
                                     name_agent::GetUpstreamInstanceRes* response,
                                     google::protobuf::Closure* done);

    void WaterCallback(etcd::Response response);
};
} // namespace name_agent