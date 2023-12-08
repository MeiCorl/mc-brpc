#pragma once

#include "net_utils.h"
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>

namespace server {
namespace utils {

class NetUtil {
public:
    /**
     * 获取指定网卡ip, 默认eth0
     * @date： 2023-08-16 15:30:00
     * @author meicorl
    */
    static void getLocalIP(char* ip, const char* icard = "eth0") {
        int inet_sock;
        struct ifreq ifr;
        inet_sock = socket(AF_INET, SOCK_DGRAM, 0);
        strcpy(ifr.ifr_name, icard);
        ioctl(inet_sock, SIOCGIFADDR, &ifr);
        strcpy(ip, inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));
    }
};

} // namespace utils
} // namespace server