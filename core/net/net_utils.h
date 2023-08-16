#pragma once

namespace server {
namespace net {

class NetUtil {
public:
    static void getLocalIP(char* ip, const char* icard = "eth0");
};

} // namespace net
} // namespace server