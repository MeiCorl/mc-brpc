#pragma once

#include <vector>
#include <unordered_map>
#include <functional>
#include <etcd/Watcher.hpp>
#include <butil/containers/doubly_buffered_data.h>
#include "name_agent.pb.h"
#include "core/config/server_config.h"
#include "core/utils/simple_timer_task.h"

namespace name_agent {

using server::config::InstanceInfo;
using server::config::ServerConfig;
using server::utils::SimpleTimerTask;
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

    void DumpServiceInfo();
public:
    AgentServiceImpl(/* args */);
    ~AgentServiceImpl();

    virtual void Test(google::protobuf::RpcController* controller,
                      const name_agent::TestReq* request,
                      name_agent::TestRes* response,
                      google::protobuf::Closure* done);
    virtual void GetServers(google::protobuf::RpcController* controller,
                            const name_agent::GetServersReq* request,
                            name_agent::GetServersRes* response,
                            google::protobuf::Closure* done);

    void WatcherCallback(etcd::Response response);
    
    // 定时任务：将服务信息dump到文件公共Prometheus基于文件服务发现（目前Prometheus暂不支持直接从etcd服务发现）
    SimpleTimerTask<AgentServiceImpl, &AgentServiceImpl::DumpServiceInfo> m_dump_task; 
};
} // namespace name_agent