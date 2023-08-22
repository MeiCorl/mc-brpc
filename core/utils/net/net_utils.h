#pragma once

namespace server {
namespace utils {

class NetUtil {
public:
    static void getLocalIP(char* ip, const char* icard = "eth0");
};

} // namespace utils
} // namespace server