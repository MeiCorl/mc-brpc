#pragma once

#include <string>

namespace server {
namespace logger {

static const std::string LOG_PATH_PREFIX      = "/data/log/";
static const std::string ASYN_LOG_PATH_PREFIX = "/data/asynlog/";
static const std::string LOG_PATH_SUFFIX      = ".log";

inline std::string get_bin_name(const char* bin_name) {
    std::string s(bin_name);
    auto const pos = s.find_last_of('/');
    if (pos != std::string::npos) {
        s = s.substr(pos + 1);
    }
    return s;
}

inline std::string get_log_path(const char* bin_name, const bool asyn_log = false) {
    if (!asyn_log) {
        return (LOG_PATH_PREFIX + get_bin_name(bin_name));
    } else {
        return (ASYN_LOG_PATH_PREFIX + get_bin_name(bin_name));
    }
}

inline std::string get_log_name(const char* bin_name, const bool asyn_log = false) {
    return (get_log_path(bin_name, asyn_log) + "/" + get_bin_name(bin_name) + LOG_PATH_SUFFIX);
}

inline std::string get_log_name(const char* bin_name,
                                const std::string sub_ins_id_str,
                                const bool asyn_log = false) {
    return (get_log_path(bin_name, asyn_log) + "/" + get_bin_name(bin_name) + sub_ins_id_str +
            LOG_PATH_SUFFIX);
}

inline std::string split_log_name(const std::string& log_file) {
    std::string s(log_file);
    auto pos = s.find_last_of("/");
    if (pos != std::string::npos) {
        s = s.substr(pos + 1);
    }

    pos = s.find_first_of(".");
    if (pos != std::string::npos) {
        s = s.substr(0, pos);
    }

    return s;
}

} // namespace logger
} // namespace server