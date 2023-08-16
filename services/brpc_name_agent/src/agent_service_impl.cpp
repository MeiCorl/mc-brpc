#include "agent_service_impl.h"
#include "core/utils/singleton.h"
#include <brpc/controller.h>
#include <etcd/Client.hpp>

using namespace name_agent;
using server::utils::Singleton;

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
    etcd::Client etcd(Singleton<ServerConfig>::get()->GetNsUrl());
    etcd::Response response = etcd.ls("").get();

    ServiceMap m1;
    ServiceRegionMap m2;
    ServiceRegionAndGroupMap m3;
    for (uint32_t i = 0; i < response.keys().size(); i++) {
        const std::string& key = response.key(i);
        string service_name    = key.substr(0, key.find(":"));
        InstanceInfo info;
        if (!info.ParseFromString(response.value(i).as_string())) {
            LOG(ERROR) << "[!] Invalid service info: " << response.value(i).as_string();
            continue;
        }
        LOG(INFO) << "[+] InitService service_name:" << service_name << ", instance: {"
                  << info.ShortDebugString() << "}";
        m1[service_name].emplace_back(info.endpoint());
        m2[service_name][info.region_id()].emplace_back(info.endpoint());
        m3[service_name][info.region_id()][info.group_id()].emplace_back(info.endpoint());
    }

    auto modify_fptr1 = [&m1](ServiceMap& map) -> int {
        for (auto it = m1.begin(); it != m1.end(); it++) {
            map[it->first].insert(map[it->first].end(), it->second.begin(), it->second.end());
        }
        return 1;
    };
    m_instances.Modify(modify_fptr1);

    auto modify_fptr2 = [&m2](ServiceRegionMap& map) -> int {
        for (auto it1 = m2.begin(); it1 != m2.end(); it1++) {
            for (auto it2 = it1->second.begin(); it2 != it1->second.end(); it2++) {
                map[it1->first][it2->first].insert(map[it1->first][it2->first].end(),
                                                   it2->second.begin(), it2->second.end());
            }
        }
        return 1;
    };
    m_instancesByRegion.Modify(modify_fptr2);

    auto modify_fptr3 = [&m3](ServiceRegionAndGroupMap& map) -> int {
        for (auto it1 = m3.begin(); it1 != m3.end(); it1++) {
            for (auto it2 = it1->second.begin(); it2 != it1->second.end(); it2++) {
                for (auto it3 = it2->second.begin(); it3 != it2->second.end(); it3++) {
                    map[it1->first][it2->first][it3->first]
                        .insert(map[it1->first][it2->first][it3->first].end(), it3->second.begin(),
                                it3->second.end());
                }
            }
        }
        return 1;
    };
    m_instancesByRegionAndGroup.Modify(modify_fptr3);
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

    const string& service_name        = request->service_name();
    server::config::ServerConfig* cfg = Singleton<ServerConfig>::get();
    uint32_t region_id                = cfg->GetSelfRegionId();
    uint32_t group_id                 = cfg->GetSelfGroupId();

    vector<string> ip_list;
    if (request->group_strategy() == GroupStrategy::STRATEGY_GROUPS_ONE_REGION) {
        // 直接本大区路由
        butil::DoublyBufferedData<ServiceRegionMap>::ScopedPtr map_ptr;
        if (m_instancesByRegion.Read(&map_ptr) != 0) {
            LOG(ERROR) << "[!] Fail to read m_instancesByRegion.";
            response->set_res_code(ServerError);
            return;
        }
        const ServiceRegionMap& m = *(map_ptr.get());

        if (m.count(service_name) != 0 && m.at(service_name).count(region_id) != 0 &&
            !m.at(service_name).at(region_id).empty()) {
            ip_list.insert(ip_list.end(), m.at(service_name).at(region_id).begin(),
                           m.at(service_name).at(region_id).end());
        } else {
            // todo 是否有指定默认大区
            response->set_res_code(NotFound);
            return;
        }
    } else if (request->group_strategy() == GroupStrategy::STRATEGY_SELF_GROUP) {
        // 仅在本机房路由
        butil::DoublyBufferedData<ServiceRegionAndGroupMap>::ScopedPtr map_ptr;
        if (m_instancesByRegionAndGroup.Read(&map_ptr) != 0) {
            LOG(ERROR) << "[!] Fail to read m_instancesByRegionAndGroup.";
            response->set_res_code(ServerError);
            return;
        }
        const ServiceRegionAndGroupMap& m = *(map_ptr.get());
        if (m.count(service_name) != 0 && m.at(service_name).count(region_id) != 0 &&
            m.at(service_name).at(region_id).count(group_id) != 0 &&
            !m.at(service_name).at(region_id).at(group_id).empty()) {
            ip_list.assign(m.at(service_name).at(region_id).at(group_id).begin(),
                           m.at(service_name).at(region_id).at(group_id).end());
        } else {
            response->set_res_code(NotFound);
            return;
        }
    } else if (request->group_strategy() == GroupStrategy::STRATEGY_CHASH_GROUPS) {
        // 先对本大区的所有机房做一致性HASH决定所返回的IP列表的机房, 再返回机房的实例IP列表路由
        butil::DoublyBufferedData<ServiceRegionAndGroupMap>::ScopedPtr map_ptr;
        if (m_instancesByRegionAndGroup.Read(&map_ptr) != 0) {
            LOG(ERROR) << "[!] Fail to read m_instancesByRegionAndGroup.";
            response->set_res_code(ServerError);
            return;
        }
        const ServiceRegionAndGroupMap& m = *(map_ptr.get());
        if (m.count(service_name) != 0 && m.at(service_name).count(region_id) != 0 &&
            !m.at(service_name).at(region_id).empty()) {
            int index = request->group_request_code() % m.at(service_name).at(region_id).size();
            auto it   = m.at(service_name).at(region_id).begin();
            for (int i = 0; i < index; i++) {
                it++;
            }
            if (it == m.at(service_name).at(region_id).end()) {
                it = m.at(service_name).at(region_id).begin();
            }
            ip_list.insert(ip_list.end(), it->second.begin(), it->second.end());
        }
    } else {
        // 优先本机房, 本机房无实例，则选取本大区
        butil::DoublyBufferedData<ServiceRegionAndGroupMap>::ScopedPtr map_ptr;
        if (m_instancesByRegionAndGroup.Read(&map_ptr) != 0) {
            LOG(ERROR) << "[!] Fail to read m_instancesByRegionAndGroup.";
            response->set_res_code(ServerError);
            return;
        }
        const ServiceRegionAndGroupMap& m = *(map_ptr.get());
        if (m.count(service_name) != 0 && m.at(service_name).count(region_id) != 0 &&
            m.at(service_name).at(region_id).count(group_id) != 0 &&
            !m.at(service_name).at(region_id).at(group_id).empty()) {
            ip_list.assign(m.at(service_name).at(region_id).at(group_id).begin(),
                           m.at(service_name).at(region_id).at(group_id).end());
        } else if (m.count(service_name) != 0 && m.at(service_name).count(region_id) != 0) {
            for (auto it = m.at(service_name).at(region_id).begin();
                 it != m.at(service_name).at(region_id).end(); it++) {
                ip_list.insert(ip_list.end(), it->second.begin(), it->second.end());
            }
        } else {
            // todo 是否有指定默认大区
            response->set_res_code(NotFound);
            return;
        }
    }

    if (ip_list.empty()) {
        response->set_res_code(NotFound);
        return;
    }

    uint32_t idx = 0;
    if (request->lb_strategy() == LbStrategy::c_murmurhash) {
        idx = request->request_code() % ip_list.size();
    } else {
        idx = m_rr_index[service_name][request->group_strategy()].fetch_add(1) % ip_list.size();
    }
    response->set_endpoint(ip_list[idx]);
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