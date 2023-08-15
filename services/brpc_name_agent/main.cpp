#include <gflags/gflags.h>
#include "core/server/MCServer.h"
#include "src/agent_service_impl.h"

int main(int argc, char* argv[]) {
    google::SetCommandLineOption("flagfile", "../conf/gflags.conf");

    server::MCServer server(argc, argv);
    name_agent::AgentServiceImpl service;

    server.AddService(&service);

    server.Start();

    return 0;
}