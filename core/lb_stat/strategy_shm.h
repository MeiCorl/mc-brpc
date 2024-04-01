#pragma once

#include "butil/endpoint.h"
#include "bthread/mutex.h"
#include "name_agent.pb.h"
#include "muti_level_hash.h"

namespace server {
namespace lb_stat {

struct ServerStats {
    uint32_t succ_cnt;
    uint32_t fail_cnt;
    uint32_t fail_net_cnt;
    uint32_t fail_logic_cnt;
    uint32_t used_ms;

    std::string service_name;
    std::string ip;
    int port;

    ServerStats() : succ_cnt(0), fail_cnt(0), fail_net_cnt(0), fail_logic_cnt(0), used_ms(0), port(0) {}
};

struct StrategyShmHead {
    int magic_num;                    // when magic number not match, mean first create shm, do memset init
    volatile short cur_shmboard_idx;  // 0 or 1
    char reserve[14];                 // reserve, only this field can be modify
};

struct StrategyShmInfo {
    char key[8];  // 4 bytes butil::ip_t, 2 bytes port
    short key_len;
    short pass_rate;
    short period;
    unsigned int cur_req_cnt;
    unsigned int strategy_time; // generate time(ms)
    char reserve[28];  // reserve, only this field can be modify
    void MakeKey(const char* ip_str, short port) {
        butil::ip_t* b_ip = (butil::ip_t*)key;
        butil::str2ip(ip_str, b_ip);
        memcpy(key + sizeof(butil::ip_t), (void*)&port, sizeof(short));
        key_len = sizeof(butil::ip_t) + sizeof(short);
    }

    int Key2EndPoint(butil::EndPoint* point) {
        if (key_len == 0) {
            return -1;
        }
        memcpy(&(point->ip), key, sizeof(butil::ip_t));
        memcpy(&(point->port), key + sizeof(butil::ip_t), sizeof(short));
        if (point->port < 0 || point->port > 65535) {
            return -2;
        }
        return 0;
    }
};

class DefaultLbStrategy;
class McLbStrategy;
class StrategyShm {
    friend DefaultLbStrategy;
    friend McLbStrategy;

public:
    StrategyShm();
    ~StrategyShm();
    static StrategyShm* GetInstance();

    /**
     * cli and nameagent all need do init with on thread, cli can do this in stat cli init
     * role: 0(name_agent)  1(cli)
     */
    int Init(int role);

    int TryAttatchShm();       // attach not create
    int AttachAndCreateShm();  // attach, if no do create
    int GetStrategy(const char* ip_str, short port, StrategyShmInfo*& info);
#ifdef LOCAL_TEST
    void PrintStrategy();
#endif
private:
    char* _shm_head;
    char* _shm_body0;
    char* _shm_body1;
    int _body_len;
    butil::atomic<bool> _init_done;

    MultiLevelHash<std::string, StrategyShmInfo>* shm_hash0;
    MultiLevelHash<std::string, StrategyShmInfo>* shm_hash1;
};

}  // namespace lb_stat
}  // namespace server