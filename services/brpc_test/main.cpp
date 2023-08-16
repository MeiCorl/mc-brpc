#include <gflags/gflags.h>
#include "core/server/MCServer.h"
#include "src/service_impl.h"

int main(int argc, char* argv[]) {
    google::SetCommandLineOption("flagfile", "../conf/gflags.conf");

    server::MCServer server(argc, argv);
    test::ServiceImpl service;

    server.AddService(&service);

    server.Start();

    return 0;
}