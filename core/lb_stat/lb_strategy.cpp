#include "lb_strategy.h"
#include "json2pb/rapidjson.h"
#include <gflags/gflags.h>

using namespace server::lb_stat;
namespace rapidjson = butil::rapidjson;

DEFINE_uint32(max_lb_strategy_live_time_ms, 30000, "max_lb_strategy_live_time_ms");

int DefaultLbStrategy::UpdateStrategy(std::unordered_map<std::string, ServerStats*>& stat_map) const {
    StrategyShm* shm = StrategyShm::GetInstance();
    if (!shm || !shm->_shm_head) {
        LOG(ERROR) << "fatal error: _shm_head is nullptr";
        return -1;
    }
    StrategyShmHead* head = (StrategyShmHead*)shm->_shm_head;

    MultiLevelHash<std::string, StrategyShmInfo>* volatile cur_hash =
        (head->cur_shmboard_idx ? shm->shm_hash1 : shm->shm_hash0);
    MultiLevelHash<std::string, StrategyShmInfo>* volatile backup_hash =
        (head->cur_shmboard_idx ? shm->shm_hash0 : shm->shm_hash1);

    uint32_t now = butil::gettimeofday_ms();
    for (uint32_t level = 0; level < cur_hash->GetMaxLevelNum(); ++level) {
        unsigned int item_num = 0;
        StrategyShmInfo* raw_hash_table = nullptr;
        int ret = cur_hash->GetRawHashTable(level, raw_hash_table, item_num);
        if (ret || !raw_hash_table) {
            LOG(ERROR) << "failed to get raw hash table!";
            return -2;
        }

        for (uint32_t k = 0; k < item_num; ++k) {
            StrategyShmInfo* shm_info = raw_hash_table + k;
            if (shm_info->key_len == 0) {
                continue;
            }

            butil::EndPoint ep;
            if (shm_info->Key2EndPoint(&ep) != 0) {
                continue;
            }
            std::string key(butil::endpoint2str(ep).c_str());

            auto it = stat_map.find(key);
            if (it != stat_map.end()) {
                // update pass_rate by stat
                uint32_t total_req_cnt = it->second->succ_cnt + it->second->fail_cnt;
                stat_map.erase(it);
                if (total_req_cnt != 0) {
                    double cur_pass_rate = 100 * (double)it->second->succ_cnt / total_req_cnt;
                    StrategyShmInfo* backup_shm_info = backup_hash->Insert(*shm_info);
                    if (backup_shm_info) {
                        backup_shm_info->cur_req_cnt = total_req_cnt;
                        backup_shm_info->pass_rate = (short)cur_pass_rate;
                        backup_shm_info->strategy_time = now;
                        continue;
                    }
                }
            }

            // when it == stat_map.end()
            if (shm_info->strategy_time + FLAGS_max_lb_strategy_live_time_ms > now) {
                backup_hash->Insert(*shm_info);
            }
        }
    }

    // handle those not found in cur_hashtable
    for (auto it = stat_map.begin(); it != stat_map.end();) {
        uint32_t total_req_cnt = it->second->succ_cnt + it->second->fail_cnt;
        if (total_req_cnt != 0) {
            double cur_pass_rate = 100 * (double)it->second->succ_cnt / total_req_cnt;
            StrategyShmInfo shm_info;
            shm_info.MakeKey(it->second->ip.c_str(), it->second->port);
            shm_info.pass_rate = (short)cur_pass_rate;
            shm_info.cur_req_cnt = total_req_cnt;
            shm_info.strategy_time = now;
            backup_hash->Insert(shm_info);
        }
        stat_map.erase(it++);
    }

    // swicth to use new hashtable, wait 20ms then reset original cur_hashtable
    head->cur_shmboard_idx = (head->cur_shmboard_idx + 1) % 2;
    usleep(20 * 1000);
    cur_hash->Reset();

#ifdef LOCAL_TEST
    shm->PrintStrategy();
#endif
    return 0;
}

McLbStrategy::McLbStrategy(const char* config_path) {
    FILE* fp = fopen(config_path, "rb");
    if (!fp) {
        LOG(FATAL) << "config file not found: " << config_path;
        exit(-1);
    }
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    rewind(fp);

    char* buffer = (char*)malloc(sizeof(char) * size);
    fread(buffer, 1, size, fp);
    rapidjson::Document doc;
    doc.Parse(buffer);
    if (doc.HasParseError()) {
        LOG(FATAL) << "invalid config file: " << config_path;
        exit(-1);
    }
    fclose(fp);
    free(buffer);

    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        const rapidjson::Value& item = it->value;
        uint32_t req_cnt = it->name.GetUint();
        for (auto iter = item.MemberBegin(); iter != item.MemberEnd(); ++iter) {
            uint32_t pass_rate = iter->name.GetUint();
            int32_t weight_incr = iter->value.GetInt();
            _strategy_config[req_cnt][pass_rate] = weight_incr;
        }
    }

    // rapidjson::StringBuffer buf;
    // rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    // doc.Accept(writer);
    // std::string str = buf.GetString();

    LOG(INFO) << "lb strategy config loaded.";
}

int McLbStrategy::UpdateStrategy(std::unordered_map<std::string, ServerStats*>& stat_map) const {
    return 0;
}