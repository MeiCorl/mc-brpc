#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_HASH_LEVEL 20

namespace server {
namespace lb_stat {

enum MultiHashErrCode {
    HASH_NOT_FOUND = -2,
    HASH_NOT_INIT = -1,
    HASH_SUCCESS = 0,
};

template <class key_type, class val_type>
class MultiLevelHash {
protected:
    unsigned int _level_num;
    unsigned int _item_num;  // item num for level 0
    void* _data_pos;
    unsigned int _data_len;
    val_type* _hash_table[MAX_HASH_LEVEL];
    unsigned int _table_item_num[MAX_HASH_LEVEL];

    bool _init_done;

public:
    MultiLevelHash(void* data_pos, unsigned int data_len, unsigned int item_num, unsigned int level_num = 4) :
            _level_num(level_num), _item_num(item_num), _data_pos(data_pos), _data_len(data_len) {
        memset((void*)_hash_table, 0, sizeof(_hash_table));
        memset((void*)_table_item_num, 0, sizeof(_table_item_num));
        _init_done = false;
    }

    int Init() {
        _init_done = false;
        if (_level_num > MAX_HASH_LEVEL || !_data_pos) {
            return -1;
        }
        val_type* pos = (val_type*)_data_pos;
        unsigned int cur_item_num = _item_num;
        for (unsigned int i = 0; i < _level_num; ++i) {
            _hash_table[i] = pos;
            _table_item_num[i] = cur_item_num;
            pos += cur_item_num;
            cur_item_num = cur_item_num / 2;
        }
        if ((void*)pos > (char*)_data_pos + _data_len) {
            return -2;
        }
        _init_done = true;
        return 0;
    }

    inline unsigned int BKDRHash(const char* str, int len, int max) {
        unsigned int seed = 131;  // 31 131 1313 13131 131313 etc..
        unsigned int hash = 0;
        for (int i = 0; i < len; ++i) {
            hash = hash * seed + (*str++);
        }
        return (hash & 0x7FFFFFFF) % max;
    }

    int Find(key_type key, val_type*& val) {
        if (!_init_done)
            return -1;
        for (unsigned int i = 0; i < _level_num; ++i) {
            val = _hash_table[i] + BKDRHash(key.c_str(), key.length(), _table_item_num[i]);
            if (val->key_len == 0)
                return -2;

            if ((unsigned int)key.length() == (unsigned int)val->key_len &&
                !memcmp(key.c_str(), val->key, val->key_len))
                return 0;
        }
        return -2;
    }

    val_type* Insert(val_type& val) {
        if (!_init_done)
            return nullptr;
        for (unsigned int i = 0; i < _level_num; ++i) {
            val_type* in_val = _hash_table[i] + BKDRHash(val.key, val.key_len, _table_item_num[i]);
            if (in_val->key_len == 0 ||
                (in_val->key_len == val.key_len && !memcmp(in_val->key, val.key, val.key_len))) {
                memcpy(in_val, &val, sizeof(val_type));
                return in_val;
            }
        }
        return nullptr;
    }

    // int Insert(val_type& val) {
    //     if (!_init_done)
    //         return -1;
    //     for (unsigned int i = 0; i < _level_num; ++i) {
    //         val_type* in_val = _hash_table[i] + BKDRHash(val.key, val.key_len, _table_item_num[i]);
    //         if (in_val->key_len == 0 ||
    //             (in_val->key_len == val.key_len && !memcmp(in_val->key, val.key, val.key_len))) {
    //             memcpy(in_val, &val, sizeof(val_type));
    //             return 0;
    //         }
    //     }
    //     return -2;
    // }

    int Clear(key_type key) {
        if (!_init_done)
            return -1;
        unsigned int find_level = _level_num;
        for (unsigned int i = 0; i < _level_num; ++i) {
            val_type* val = _hash_table[i] + BKDRHash(key.c_str(), key.length(), _table_item_num[i]);
            if (val->key_len == 0)
                return -2;
            if ((unsigned int)key.length() == (unsigned int)val->key_len &&
                !memcmp(key.c_str(), val->key, val->key_len)) {
                memset(val, 0, sizeof(val_type));
                find_level = i;
                break;
            }
        }

        // move next level value to prev-level
        for (unsigned int cur_level = find_level; cur_level < _level_num - 1; ++cur_level) {
            unsigned int next_level = cur_level + 1;
            val_type* cur_val =
                _hash_table[cur_level] + BKDRHash(key.c_str(), key.length(), _table_item_num[cur_level]);
            val_type* next_val =
                _hash_table[next_level] + BKDRHash(key.c_str(), key.length(), _table_item_num[next_level]);
            if (next_val->key_len == 0) {
                return 0;
            }
            memcpy(cur_val, next_val, sizeof(val_type));
        }

        if (find_level < _level_num)
            return 0;

        return -2;
    }

    // return raw hash table
    int GetRawHashTable(unsigned int level_idx, val_type*& raw_hash_table, unsigned int& item_num) {
        if (level_idx >= _level_num) {
            return -1;
        }
        raw_hash_table = _hash_table[level_idx];
        item_num = _table_item_num[level_idx];
        return 0;
    }

    unsigned int GetMaxLevelNum() {
        return _level_num;
    }

    void Reset() {
        if (!_init_done)
            return;

        memset(_data_pos, 0, _data_len);
    }
};

}  // namespace lb_stat
}  // namespace server
