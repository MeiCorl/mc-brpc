#include "core/server/MCServer.h"
#include "src/service_impl.h"

int main(int argc, char* argv[]) {
    server::MCServer server(argc, argv);
    test::ServiceImpl service;

    server.AddService(&service);

    server.Start();

    return 0;
}