#include "core/server/MCServer.h"
#include "src/service_impl.h"
#include "core/common/metrcis_helper.h"

DEFINE_METRICS_counter_u64(service_request_counter, "service_request_counter", "service_request_counter");

int main(int argc, char* argv[]) {
    server::MCServer server(argc, argv);
    test::ServiceImpl service;

    server.AddService(&service);

    ADD_STATIC_COUNTER_LABEL(service_request_counter, "service", "brpc_test");
    ADD_COUNTER_LABEL(service_request_counter, "method");

    server.Start();

    return 0;
}