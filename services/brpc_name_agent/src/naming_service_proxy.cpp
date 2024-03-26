#include "naming_service_proxy.h"
#include "core/utils/singleton.h"
#include "core/utils/json_util.h"
#include "bthread/bthread.h"
#include <gflags/gflags.h>
#include <etcd/Client.hpp>
#include "json2pb/rapidjson.h"

using namespace name_agent;
using namespace server::utils;
namespace rapidjson = butil::rapidjson;

DEFINE_uint32(prometheus_targets_dump_interval, 10, "prometheus targets dump interval(seconds)");
DEFINE_string(
    prometheus_targets_file,
    "instance.json",
    "the file to store instances of prometheus file_sd_config service discovery.");

const std::string NAME_AGENT_DUMP_LOCK = "name_service_dump_lock";

void NameServiceProxy::InitializeWatcher(
    const string& endpoints,
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

void NameServiceProxy::InitServers() {
    etcd::Client etcd(ServerConfig::GetInstance()->GetNsUrl());
    etcd::Response response = etcd.ls("").get();

    for (uint32_t i = 0; i < response.keys().size(); i++) {
        const std::string& key = response.key(i);
        if (key == NAME_AGENT_DUMP_LOCK) {
            continue;
        }

        string service_name = key.substr(0, key.find(":"));
        InstanceInfo info;
        if (!info.ParseFromString(response.value(i).as_string())) {
            LOG(ERROR) << "[!] Invalid service info: " << response.value(i).as_string();
            continue;
        }
        AddServer(service_name, info);
    }
}

void NameServiceProxy::AddServer(const string& service_name, const InstanceInfo& info) {
    {
        // 去重
        BAIDU_SCOPED_LOCK(m_mtx);
        if (m_endPoints.count(info.endpoint())) {
            return;
        }
        m_endPoints.insert(info.endpoint());
    }

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

    LOG(INFO) << "[+] AddServer service:" << service_name << " instance_info:" << info.ShortDebugString();
}

void NameServiceProxy::RemoveServer(const string& service_name, const InstanceInfo& info) {
    auto modify_fptr1 = [&service_name, &info](ServiceMap& map) -> int {
        if (map.count(service_name)) {
            map[service_name].erase(
                std::remove_if(
                    map[service_name].begin(),
                    map[service_name].end(),
                    [&](const string& endpoint) { return endpoint == info.endpoint(); }),
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
            map[service_name][info.region_id()].erase(
                std::remove_if(
                    map[service_name][info.region_id()].begin(),
                    map[service_name][info.region_id()].end(),
                    [&](const string& endpoint) { return endpoint == info.endpoint(); }),
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
            map[service_name][info.region_id()][info.group_id()].erase(
                std::remove_if(
                    map[service_name][info.region_id()][info.group_id()].begin(),
                    map[service_name][info.region_id()][info.group_id()].end(),
                    [&](const string& endpoint) { return endpoint == info.endpoint(); }),
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

    {
        BAIDU_SCOPED_LOCK(m_mtx);
        m_endPoints.erase(info.endpoint());
    }
    LOG(INFO) << "[+] RemoveServer service:" << service_name << " instance_info:" << info.ShortDebugString();
}

void NameServiceProxy::WatcherCallback(etcd::Response response) {
    for (const etcd::Event& ev : response.events()) {
        string key = ev.kv().key();
        if (key == NAME_AGENT_DUMP_LOCK) {
            return;
        }
        string service_name = key.substr(0, key.find(":"));

        if (ev.event_type() == etcd::Event::EventType::PUT) {
            InstanceInfo info;
            if (!info.ParseFromString(ev.kv().as_string())) {
                LOG(ERROR) << "[!] Invalid service info, key:" << ev.kv().key() << " , value:" << ev.kv().as_string();
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
            LOG(ERROR) << "[!] Invalid event type:" << static_cast<int>(ev.event_type()) << " key:" << ev.kv().key()
                       << " value:" << ev.kv().as_string();
        }
    }
}

void NameServiceProxy::DumpServiceInfo() {
    // 选主执行，当有多个brpc_name_agnet实例时，只需要有一个实例执行dump任务就行
    // 支持某个nage_agent进程挂掉后，下次会由另一个name_agent进程执行dump任务
    ServerConfig* config = ServerConfig::GetInstance();
    etcd::Client etcd(config->GetNsUrl());
    etcd::Response resp1 = etcd.leasegrant(FLAGS_prometheus_targets_dump_interval - 1).get();
    if (resp1.error_code() != 0) {
        LOG(ERROR) << "[!] etcd failed, err_code: " << resp1.error_code() << ", err_msg:" << resp1.error_message();
        return;
    }
    int64_t lease_id = resp1.value().lease();
    etcd::Response resp2 =
        etcd.add(NAME_AGENT_DUMP_LOCK, "dump_lock", lease_id).get();  // key存在会失败, 只有第一个add会成功
    if (resp2.error_code() != 0) {
        return;
    }

    butil::DoublyBufferedData<ServiceRegionAndGroupMap>::ScopedPtr map_ptr;
    if (m_instancesByRegionAndGroup.Read(&map_ptr) != 0) {
        LOG(ERROR) << "[!] Fail to read m_instancesByRegionAndGroup.";
        return;
    }
    const ServiceRegionAndGroupMap& m = *(map_ptr.get());

    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);
    writer.StartArray();
    for (auto it1 = m.begin(); it1 != m.end(); it1++) {
        const std::string& service_name = it1->first;
        for (auto it2 = it1->second.begin(); it2 != it1->second.end(); it2++) {
            uint32_t region_id = it2->first;

            writer.StartObject();
            for (auto it3 = it2->second.begin(); it3 != it2->second.end(); it3++) {
                uint32_t group_id = it3->first;
                writer.Key("targets");
                writer.StartArray();
                for (const std::string& endpoint : it3->second) {
                    writer.String(endpoint.c_str());
                }
                writer.EndArray();

                writer.Key("labels");
                writer.StartObject();
                writer.Key("server_name");
                writer.String(service_name.c_str());
                writer.Key("region_id");
                writer.String(std::to_string(region_id).c_str());
                writer.Key("group_id");
                writer.String(std::to_string(group_id).c_str());
                writer.EndObject();
            }
            writer.EndObject();
        }
    }
    writer.EndArray();

    std::string json = formatJson(strBuf.GetString());
    FILE* fp = fopen(FLAGS_prometheus_targets_file.c_str(), "w");
    if (fp != NULL) {
        fwrite(json.data(), json.size(), 1, fp);
        fflush(fp);
        fclose(fp);
        LOG(INFO) << FLAGS_prometheus_targets_file << " updated.";
    } else {
        LOG(ERROR) << "Fail to write service info to " << FLAGS_prometheus_targets_file;
    }
}

NameServiceProxy::NameServiceProxy(/* args */) {
    InitServers();

    std::function<void(etcd::Response)> callback =
        bind(&NameServiceProxy::WatcherCallback, this, std::placeholders::_1);
    InitializeWatcher(ServerConfig::GetInstance()->GetNsUrl(), "", callback, m_pEtcdWatcher);

    m_dump_task.Init(this, FLAGS_prometheus_targets_dump_interval);
    m_dump_task.Start();
}

ResCode NameServiceProxy::GetServers(
    const string& service_name,
    uint32_t group_strategy,
    uint32_t group_request_code,
    google::protobuf::RepeatedPtrField<string>* endpoints) {
    server::config::ServerConfig* cfg = ServerConfig::GetInstance();
    uint32_t region_id = cfg->GetSelfRegionId();
    uint32_t group_id = cfg->GetSelfGroupId();

    if (group_strategy == server::config::GroupStrategy::STRATEGY_GROUPS_ONE_REGION) {
        // 直接本大区路由
        butil::DoublyBufferedData<ServiceRegionMap>::ScopedPtr map_ptr;
        if (m_instancesByRegion.Read(&map_ptr) != 0) {
            LOG(ERROR) << "[!] Fail to read m_instancesByRegion.";
            return ServerError;
        }
        const ServiceRegionMap& m = *(map_ptr.get());

        if (m.count(service_name) != 0 && m.at(service_name).count(region_id) != 0 &&
            !m.at(service_name).at(region_id).empty()) {
            google::protobuf::RepeatedPtrField<string> eps(
                m.at(service_name).at(region_id).begin(), m.at(service_name).at(region_id).end());
            endpoints->Swap(&eps);
            return Success;
        } else {
            // todo 是否有指定默认大区
            return NotFound;
        }
    } else if (group_strategy == server::config::GroupStrategy::STRATEGY_SELF_GROUP) {
        // 仅在本机房路由
        butil::DoublyBufferedData<ServiceRegionAndGroupMap>::ScopedPtr map_ptr;
        if (m_instancesByRegionAndGroup.Read(&map_ptr) != 0) {
            LOG(ERROR) << "[!] Fail to read m_instancesByRegionAndGroup.";
            return ServerError;
        }
        const ServiceRegionAndGroupMap& m = *(map_ptr.get());
        if (m.count(service_name) != 0 && m.at(service_name).count(region_id) != 0 &&
            m.at(service_name).at(region_id).count(group_id) != 0 &&
            !m.at(service_name).at(region_id).at(group_id).empty()) {
            google::protobuf::RepeatedPtrField<string> eps(
                m.at(service_name).at(region_id).at(group_id).begin(),
                m.at(service_name).at(region_id).at(group_id).end());
            endpoints->Swap(&eps);
            return Success;
        } else {
            return NotFound;
        }
        // } else if (group_strategy == server::config::GroupStrategy::STRATEGY_CHASH_GROUPS) {
        //     // 先对本大区的所有机房做一致性HASH决定所返回的IP列表的机房, 再返回机房的实例IP列表路由
        //     butil::DoublyBufferedData<ServiceRegionAndGroupMap>::ScopedPtr map_ptr;
        //     if (m_instancesByRegionAndGroup.Read(&map_ptr) != 0) {
        //         LOG(ERROR) << "[!] Fail to read m_instancesByRegionAndGroup.";
        //         response->set_res_code(ServerError);
        //         return;
        //     }
        //     const ServiceRegionAndGroupMap& m = *(map_ptr.get());
        //     if (m.count(service_name) != 0 && m.at(service_name).count(region_id) != 0 &&
        //         !m.at(service_name).at(region_id).empty()) {
        //         int index = request->group_request_code() % m.at(service_name).at(region_id).size();
        //         auto it   = m.at(service_name).at(region_id).begin();
        //         for (int i = 0; i < index; i++) {
        //             it++;
        //         }
        //         if (it == m.at(service_name).at(region_id).end()) {
        //             it = m.at(service_name).at(region_id).begin();
        //         }

        //         google::protobuf::RepeatedPtrField<string> endpoints(it->second.begin(),
        //                                                              it->second.end());
        //         response->mutable_endpoints()->Swap(&endpoints);
        //     }
        // }
    } else {
        // 优先本机房, 本机房无实例，则选取本大区
        butil::DoublyBufferedData<ServiceRegionAndGroupMap>::ScopedPtr map_ptr;
        if (m_instancesByRegionAndGroup.Read(&map_ptr) != 0) {
            LOG(ERROR) << "[!] Fail to read m_instancesByRegionAndGroup.";
            return ServerError;
        }
        const ServiceRegionAndGroupMap& m = *(map_ptr.get());
        if (m.count(service_name) != 0 && m.at(service_name).count(region_id) != 0 &&
            m.at(service_name).at(region_id).count(group_id) != 0 &&
            !m.at(service_name).at(region_id).at(group_id).empty()) {
            google::protobuf::RepeatedPtrField<string> eps(
                m.at(service_name).at(region_id).at(group_id).begin(),
                m.at(service_name).at(region_id).at(group_id).end());
            endpoints->Swap(&eps);
            return Success;
        } else if (m.count(service_name) != 0 && m.at(service_name).count(region_id) != 0) {
            for (auto it = m.at(service_name).at(region_id).begin(); it != m.at(service_name).at(region_id).end();
                 it++) {
                for (const string& endpoint : it->second) {
                    endpoints->Add()->assign(endpoint);
                }
            }
            return Success;
        } else {
            // todo 是否有指定默认大区
            return NotFound;
        }
    }
}