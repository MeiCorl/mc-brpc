#include "strategy_shm.h"
#include "butil/memory/singleton.h"
#include <sys/shm.h>

using namespace server::lb_stat;

#define LB_STRATEGY_SHM_KEY    0x53CBD1
#define LB_STRATEGY_SHM_MAGIC  0x3941591
#define LB_STRATEGY_SHM_LEN    51 * 1024 * 1024
#define LB_STRATEGY_HASH_ITEMS 250000

StrategyShm* StrategyShm::GetInstance() {
    return Singleton<StrategyShm>::get();
}

StrategyShm::StrategyShm() :
        _shm_head(nullptr), _shm_body0(nullptr), _shm_body1(nullptr), _body_len(0), _init_done(false) {}

StrategyShm::~StrategyShm() {
    if (_shm_head) {
        shmdt(_shm_head);
        _shm_head = nullptr;
    }
    if (shm_hash0) {
        delete shm_hash0;
        shm_hash0 = nullptr;
    }
    if (shm_hash1) {
        delete shm_hash1;
        shm_hash1 = nullptr;
    }
}

int StrategyShm::Init(int role) {
    int ret = 0;
    if (role == 1) {
        // cli
        ret = TryAttatchShm();
    } else {
        // name_agent
        ret = AttachAndCreateShm();
    }
    if (ret) {
        LOG(ERROR) << "AttatchShm failed, role:" << role;
        return ret;
    }
    assert(_shm_body0);
    assert(_shm_body1);

    shm_hash0 = new MultiLevelHash<std::string, StrategyShmInfo>(_shm_body0, _body_len, LB_STRATEGY_HASH_ITEMS);
    shm_hash1 = new MultiLevelHash<std::string, StrategyShmInfo>(_shm_body1, _body_len, LB_STRATEGY_HASH_ITEMS);

    assert(shm_hash0);
    assert(shm_hash1);
    ret = shm_hash0->Init();
    if (ret) {
        LOG(ERROR) << "shm_hash0->Init fail, ret:" << ret;
        return ret;
    }
    ret = shm_hash1->Init();
    if (ret) {
        LOG(ERROR) << "shm_hash1->Init fail, ret:" << ret;
        return ret;
    }
    _init_done.store(true, butil::memory_order_release);
    LOG(INFO) << "strategy shm inited, magic_num:" << *(int*)_shm_head;
    return 0;
}

int StrategyShm::TryAttatchShm() {
    int shmid = shmget((key_t)LB_STRATEGY_SHM_KEY, LB_STRATEGY_SHM_LEN, 0666);
    if (shmid < 0) {
        LOG(ERROR) << "err when  shmget, errno:" << errno << ", LB_STRATEGY_SHM_LEN:" << LB_STRATEGY_SHM_LEN
                   << ", shmkey:" << LB_STRATEGY_SHM_KEY << ", shmid:" << shmid;
        return -1;
    }

    _shm_head = (char*)shmat(shmid, nullptr, 0);
    if (_shm_head < (char*)0) {
        LOG(ERROR) << "err when  shmat, errno:" << errno;
        return -2;
    }

    // check data head,if not match,attach fail
    int magic = *(int*)_shm_head;
    if (magic != LB_STRATEGY_SHM_MAGIC) {
        LOG(ERROR) << "check magic err, " << magic << ":" << LB_STRATEGY_SHM_MAGIC;
        return -3;
    }

    _body_len = LB_STRATEGY_HASH_ITEMS * 2 * sizeof(StrategyShmInfo);
    // assert(50 == sizeof(StrategyShmInfo));  // no body modify this size
    assert(_body_len * 2 + sizeof(StrategyShmHead) < LB_STRATEGY_SHM_LEN);
    _shm_body0 = _shm_head + sizeof(StrategyShmHead);
    _shm_body1 = _shm_head + sizeof(StrategyShmHead) + _body_len;

    return 0;
}

int StrategyShm::AttachAndCreateShm() {
    int shmid = shmget((key_t)LB_STRATEGY_SHM_KEY, LB_STRATEGY_SHM_LEN, IPC_CREAT | 0666);
    if (shmid < 0) {
        LOG(ERROR) << "err when shmget, errno:" << errno << ", LB_STRATEGY_SHM_LEN:" << LB_STRATEGY_SHM_LEN
                   << ", shmkey:" << LB_STRATEGY_SHM_KEY << ", shmid:" << shmid;
        return -1;
    }

    _shm_head = (char*)shmat(shmid, nullptr, 0);
    if (_shm_head < (char*)0) {
        LOG(ERROR) << "err when shmat, errno:" << errno;
        return -2;
    }

    // check data head, if first time use, needs to clear data
    int magic = *(int*)_shm_head;
    if (magic != LB_STRATEGY_SHM_MAGIC) {
        memset(_shm_head, 0, LB_STRATEGY_SHM_LEN);
        *(int*)_shm_head = LB_STRATEGY_SHM_MAGIC;
    }

    _body_len = LB_STRATEGY_HASH_ITEMS * 2 * sizeof(StrategyShmInfo);
    // assert(50 == sizeof(StrategyShmInfo));  // no body modify this size
    assert(_body_len * 2 + sizeof(StrategyShmHead) < LB_STRATEGY_SHM_LEN);
    _shm_body0 = _shm_head + sizeof(StrategyShmHead);
    _shm_body1 = _shm_head + sizeof(StrategyShmHead) + _body_len;

    return 0;
}

int StrategyShm::GetStrategy(const char* ip_str, short port, StrategyShmInfo*& info) {
    return 0;
}

#ifdef LOCAL_TEST
void StrategyShm::PrintStrategy() {
    if (!_shm_head) {
        LOG(ERROR) << "fatal error: _shm_head is nullptr";
        return;
    }
    StrategyShmHead* head = (StrategyShmHead*)_shm_head;

    MultiLevelHash<std::string, StrategyShmInfo>* volatile cur_hash = (head->cur_shmboard_idx ? shm_hash1 : shm_hash0);
    std::ostringstream os;
    os << "[";
    bool flag = false;
    for (uint32_t level = 0; level < cur_hash->GetMaxLevelNum(); ++level) {
        unsigned int item_num = 0;
        StrategyShmInfo* raw_hash_table = nullptr;
        int ret = cur_hash->GetRawHashTable(level, raw_hash_table, item_num);
        if (ret || !raw_hash_table) {
            LOG(ERROR) << "failed to get raw hash table!";
            return;
        }

        for (uint32_t k = 0; k < item_num; ++k) {
            StrategyShmInfo* shm_info = raw_hash_table + k;
            if (shm_info->key_len == 0) {
                continue;
            }
            if (flag) {
                os << ", ";
            }
            butil::EndPoint ep;
            shm_info->Key2EndPoint(&ep);
            os << "{\"endpoint\":\"" << butil::endpoint2str(ep).c_str() << "\", \"pass_rate\":" << shm_info->pass_rate
               << ", \"strategy_time\":" << shm_info->strategy_time << "}";
            flag = true;
        }
    }
    os << "]";
    LOG(INFO) << "cur lb strategy: " << os.str();
}
#endif