#include "etcd_service_register.h"
#include "core/utils/singleton.h"
#include "butil/logging.h"
#include "brpc/server.h"
#include <etcd/Client.hpp>

using namespace brpc::policy;

std::string EtcdServiceRegister::BuildServiceName(
    const std::string& original_service_name,
    const server::config::InstanceInfo& instance) {
    std::hash<std::string> hasher;
    return original_service_name + ":" + std::to_string(hasher(instance.SerializeAsString()));
}

bool EtcdServiceRegister::RegisterService() {
    ServerConfig* config = ServerConfig::GetInstance();
    etcd::Client etcd(config->GetNsUrl());
    server::config::InstanceInfo instance;
    instance.set_region_id(config->GetSelfRegionId());
    instance.set_group_id(config->GetSelfGroupId());
    instance.set_endpoint(butil::endpoint2str(brpc::Server::_current_server->listen_address()).c_str());
    etcd::Response resp = etcd.leasegrant(REGISTER_TTL).get();
    if (resp.error_code() != 0) {
        LOG(ERROR) << "[!] etcd failed, err_code: " << resp.error_code() << ", err_msg:" << resp.error_message();
        return false;
    }
    _etcd_lease_id = resp.value().lease();

    _register_key = BuildServiceName(config->GetSelfName(), instance);
    etcd::Response response = etcd.set(_register_key, instance.SerializeAsString(), _etcd_lease_id).get();
    if (response.error_code() != 0) {
        LOG(ERROR) << "[!] Fail to register service, err_code: " << response.error_code()
                   << ", err_msg:" << response.error_message();
        return false;
    }

    std::function<void(std::exception_ptr)> handler = [this](std::exception_ptr eptr) {
        try {
            if (eptr) {
                std::rethrow_exception(eptr);
            }
        } catch (const std::runtime_error& e) {
            LOG(FATAL) << "[!] Etcd connection failure: " << e.what();
        } catch (const std::out_of_range& e) {
            LOG(FATAL) << "[!] Etcd lease expire: " << e.what();
        }

        // KeepAlive续约有时因为一些原因lease已经过期则会失败，或者etcd重启也会失败，需要发起重新注册
        // 注意需要新起一个线程发起重新注册，否则会出现死锁（不新起线程的话，那重新注册的逻辑是运行在当前
        // handler所在线程的，而旧的KeepAlive对象析构又需要等待当前handler所在线程结束，具体请看KeepAlive源码）
        auto fn = std::bind(&EtcdServiceRegister::TryRegisterAgain, this);
        std::thread(fn).detach();
    };

    // 注册信息到期前5s进行续约，避免租约到期后续约失败
    _keep_live_ptr.reset(new etcd::KeepAlive(config->GetNsUrl(), handler, REGISTER_TTL - 5, _etcd_lease_id));
    LOG(INFO) << "Service register succ. instance: {" << instance.ShortDebugString()
              << "}, lease_id:" << _etcd_lease_id;
    return true;
}

/**
 * 用于在一些异常情况下(如etcd重启或者旧的lease过期导致keepalive对象失败退出)重新发起注册
 **/
void EtcdServiceRegister::TryRegisterAgain() {
    do {
        LOG(INFO) << "Try register again.";
        bool is_succ = RegisterService();
        if (is_succ) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    } while (true);
}

void EtcdServiceRegister::UnRegisterService() {
    if (_keep_live_ptr) {
        _keep_live_ptr->Cancel();
    }

    ServerConfig* config = ServerConfig::GetInstance();
    etcd::Client etcd(config->GetNsUrl());
    etcd.rm(_register_key);
}