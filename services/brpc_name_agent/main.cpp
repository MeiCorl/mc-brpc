#include <gflags/gflags.h>
#include "core/server/MCServer.h"
#include "src/agent_service_impl.h"

int main(int argc, char* argv[]) {
    google::SetCommandLineOption("flagfile", "../conf/gflags.conf");

    server::MCServer server(argc, argv);
    name_agent::AgentServiceImpl service;

    server.AddService(&service);

    // name_agent作为基础服务打包到docker镜像，通过unix_socket通信，不用注册到服务中心
    server.Start(false);

    return 0;
}