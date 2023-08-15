#include "agent_service_impl.h"
#include "core/utils/singleton.h"
#include <brpc/controller.h>
#include <etcd/Client.hpp>

using namespace name_agent;

// DEFINE_string(ns_url,
//               "http://127.0.0.1:2379",
//               "address of name service."
//               "address of name service.");

void AgentServiceImpl::InitializeWatcher(const string& endpoints,
                                         const string& prefix,
                                         std::function<void(etcd::Response)> callback,
                                         shared_ptr<etcd::Watcher>& watcher) {
    etcd::Client client(endpoints);
    // wait until the client connects to etcd server
    while (!client.head().get().is_ok()) {
        sleep(1);
    }

    // Check if the failed one has been cancelled first
    if (watcher && watcher->Cancelled()) {
        LOG(INFO) << "[+] Watcher's reconnect loop been cancelled.";
        return;
    }
    watcher.reset(new etcd::Watcher(client, prefix, callback, true));

    // Note that lambda requires `mutable`qualifier.
    watcher->Wait([endpoints, prefix, callback, &watcher, this](bool cancelled) mutable {
        if (cancelled) {
            LOG(INFO) << "[+] Watcher's reconnect loop stopped as been cancelled.";
            return;
        }
        this->InitializeWatcher(endpoints, prefix, callback, watcher);
    });

    LOG(INFO) << "[+] Etcd watcher started.";
}

void AgentServiceImpl::InitServers() {
    etcd::Client etcd(server::utils::Singleton<ServerConfig>::get()->GetNsUrl());
    etcd::Response response = etcd.ls("/").get();

    ServiceMap m1;
    ServiceRegionMap m2;
    ServiceRegionAndGroupMap m3;
    for (uint32_t i = 0; i < response.keys().size(); i++) {
        const string& service_name = response.key(i);
        InstanceInfo info;
        if (!info.ParseFromString(response.value(i).as_string())) {
            LOG(ERROR) << "[!] Invalid service info: " << response.value(i).as_string();
            continue;
        }
        m1[service_name].emplace_back(info.endpoint());
        m2[service_name][info.region_id()].emplace_back(info.endpoint());
        m3[service_name][info.region_id()][info.group_id()].emplace_back(info.endpoint());
    }

    auto modify_fptr1 = [&m1](ServiceMap& map) -> int {
        map.swap(m1);
        return 1;
    };
    m_instances.Modify(modify_fptr1);

    auto modify_fptr2 = [&m2](ServiceRegionMap& map) -> int {
        map.swap(m2);
        return 1;
    };
    m_instancesByRegion.Modify(modify_fptr2);

    auto modify_fptr3 = [&m3](ServiceRegionAndGroupMap& map) -> int {
        map.swap(m3);
        return 1;
    };
    m_instancesByRegionAndGroup.Modify(modify_fptr3);

    LOG(INFO) << "[+] InitServers ...";
}

void AgentServiceImpl::AddServer(const string& service_name, const InstanceInfo& info) {
    auto modify_fptr1 = [&service_name, &info](ServiceMap& map) -> int {
        map[service_name].emplace_back(info.endpoint());
        return 1;
    };
    m_instances.Modify(modify_fptr1);

    auto modify_fptr2 = [&service_name, &info](ServiceRegionMap& map) -> int {
        map[service_name][info.region_id()].emplace_back(info.endpoint());
        return 1;
    };
    m_instancesByRegion.Modify(modify_fptr2);

    auto modify_fptr3 = [&service_name, &info](ServiceRegionAndGroupMap& map) -> int {
        map[service_name][info.region_id()][info.group_id()].emplace_back(info.endpoint());
        return 1;
    };
    m_instancesByRegionAndGroup.Modify(modify_fptr3);

    LOG(INFO) << "[+] AddServer service:" << service_name
              << " instance_info:" << info.ShortDebugString();
}

void AgentServiceImpl::RemoveServer(const string& service_name, const InstanceInfo& info) {
    auto modify_fptr1 = [&service_name, &info](ServiceMap& map) -> int {
        if (map.count(service_name)) {
            map[service_name].erase(std::remove_if(map[service_name].begin(), map[service_name].end(),
                                                   [&](const string& endpoint) {
                                                       return endpoint == info.endpoint();
                                                   }),
                                    map[service_name].end());
            if (map[service_name].empty()) {
                map.erase(service_name);
            }
        }
        return 1;
    };
    m_instances.Modify(modify_fptr1);


    auto modify_fptr2 = [&service_name, &info](ServiceRegionMap& map) -> int {
        if (map.count(service_name) && map[service_name].count(info.region_id())) {
            map[service_name][info.region_id()]
                .erase(std::remove_if(map[service_name][info.region_id()].begin(),
                                      map[service_name][info.region_id()].end(),
                                      [&](const string& endpoint) {
                                          return endpoint == info.endpoint();
                                      }),
                       map[service_name][info.region_id()].end());
            if (map[service_name][info.region_id()].empty()) {
                map[service_name].erase(info.region_id());
            }

            if (map[service_name].empty()) {
                map.erase(service_name);
            }
        }
        return 1;
    };
    m_instancesByRegion.Modify(modify_fptr2);


    auto modify_fptr3 = [&service_name, &info](ServiceRegionAndGroupMap& map) -> int {
        if (map.count(service_name) && map[service_name].count(info.region_id()) &&
            map[service_name][info.region_id()].count(info.group_id())) {
            map[service_name][info.region_id()][info.group_id()]
                .erase(std::remove_if(map[service_name][info.region_id()][info.group_id()].begin(),
                                      map[service_name][info.region_id()][info.group_id()].end(),
                                      [&](const string& endpoint) {
                                          return endpoint == info.endpoint();
                                      }),
                       map[service_name][info.region_id()][info.group_id()].end());
            if (map[service_name][info.region_id()][info.group_id()].empty()) {
                map[service_name][info.region_id()].erase(info.group_id());
            }

            if (map[service_name][info.region_id()].empty()) {
                map[service_name].erase(info.region_id());
            }

            if (map[service_name].empty()) {
                map.erase(service_name);
            }
        }
        return 1;
    };
    m_instancesByRegionAndGroup.Modify(modify_fptr3);

    LOG(INFO) << "[+] RemoveServer service:" << service_name
              << " instance_info:" << info.ShortDebugString();
}

AgentServiceImpl::AgentServiceImpl(/* args */) {
    InitServers();

    std::function<void(etcd::Response)> callback =
        bind(&AgentServiceImpl::WaterCallback, this, std::placeholders::_1);
    InitializeWatcher(server::utils::Singleton<ServerConfig>::get()->GetNsUrl(), "", callback,
                      m_pEtcdWatcher);
}

AgentServiceImpl::~AgentServiceImpl() { }

void AgentServiceImpl::Test(google::protobuf::RpcController* controller,
                            const name_agent::TestReq* request,
                            name_agent::TestRes* response,
                            google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_seq_id(request->seq_id());
    response->set_res_code(Success);
    response->set_res_msg(request->msg());
    butil::DoublyBufferedData<ServiceMap>::ScopedPtr map_ptr;
    if (m_instances.Read(&map_ptr) != 0) {
        LOG(INFO) << "[!] Fail to read m_instances.";
    } else {
        for (auto it = map_ptr->begin(); it != map_ptr->end(); it++) {
            LOG(INFO) << "[+] service_name:" << it->first << ", instance_num:" << it->second.size();
        }
    }
}

void AgentServiceImpl::GetUpstreamInstance(google::protobuf::RpcController* controller,
                                           const name_agent::GetUpstreamInstanceReq* request,
                                           name_agent::GetUpstreamInstanceRes* response,
                                           google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_seq_id(request->seq_id());
    response->set_res_code(Success);
}

void AgentServiceImpl::WaterCallback(etcd::Response response) {
    for (const etcd::Event& ev : response.events()) {
        string key          = ev.kv().key();
        string service_name = key.substr(0, key.find(":"));

        if (ev.event_type() == etcd::Event::EventType::PUT) {
            InstanceInfo info;
            if (!info.ParseFromString(ev.kv().as_string())) {
                LOG(ERROR) << "[!] Invalid service info, key:" << ev.kv().key()
                           << " , value:" << ev.kv().as_string();
                continue;
            }

            AddServer(service_name, info);
        } else if (ev.event_type() == etcd::Event::EventType::DELETE_) {
            InstanceInfo info;
            if (!info.ParseFromString(ev.prev_kv().as_string())) {
                LOG(ERROR) << "[!] Invalid service info: " << ev.prev_kv().as_string();
                continue;
            }

            RemoveServer(service_name, info);
        } else {
            LOG(ERROR) << "[!] Invalid event type:" << static_cast<int>(ev.event_type())
                       << " key:" << ev.kv().key() << " value:" << ev.kv().as_string();
        }
    }
}