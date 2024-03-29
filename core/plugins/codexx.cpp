/**
 * Rpc客户端代码生成插件
 * 根据proto文件自动生成对应服务的客户端类（SyncClient、ASyncClient、 SemiSyncClient)
 * XxxClient（即对rpc接口及channel、stub、controller的封装整合)
 * 使用方法及效果参见: services/brpc_test
 * build cmd: g++ codexx.cpp -lprotoc -lprotobuf -o codexx
 * @date: 2023/08/22 16:00:30
 * @author: meicorl
 */

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/io/printer.h>
#include <string>
#include <vector>

using namespace google::protobuf;
using namespace google::protobuf::compiler;
using namespace google::protobuf::io;

class CodeXXGenerator : public google::protobuf::compiler::CodeGenerator {
public:
    virtual bool Generate(
        const FileDescriptor* file,
        const string& parameter,
        GeneratorContext* generator_context,
        string* error) const {
        GenerateHeaderFile(file, generator_context);
        GenerateSourceFile(file, generator_context);
        return true;
    }

private:
    void split(const std::string& str, std::vector<std::string>& results, const std::string& del = ".") const {
        int start = str.find_first_not_of(del, 0);
        int end = str.find_first_of(del, start);
        while ((start != string::npos) || (end != string::npos)) {
            results.emplace_back(str.substr(start, end - start));
            start = str.find_first_not_of(del, end);
            end = str.find_first_of(del, start);
        }
    }

    void GenerateHeaderFile(const FileDescriptor* file, GeneratorContext* generator_context) const {
        std::string file_name = file->name().substr(0, file->name().find('.'));
        std::string header_file_name = file_name + ".client.h";
        auto* outstream = generator_context->Open(header_file_name);

        Printer printer(outstream, '$');
        printer.PrintRaw(notic);
        printer.PrintRaw("#pragma once\n\n");

        printer.PrintRaw("#include <brpc/controller.h>\n");
        printer.PrintRaw("#include <brpc/channel.h>\n");
        printer.PrintRaw("#include \"" + file_name + ".pb.h\"\n");
        printer.PrintRaw("#include \"core/config/server_config.h\"\n");

        // namespace begin
        printer.PrintRaw("\n");
        std::vector<std::string> namespaces;
        split(file->package(), namespaces);
        for (const std::string& ns : namespaces) {
            printer.PrintRaw("namespace " + ns + " { \n");
        }
        printer.PrintRaw("using namespace server::config;\n");
        printer.PrintRaw("\n\n");

        std::vector<std::string> client_types = {"SyncClient", "ASyncClient", "SemiSyncClient"};
        for (const std::string& client_type : client_types) {
            printer.Print("class $client_type$ {\n", "client_type", client_type);
            if (client_type == "ASyncClient") {
                printer.PrintRaw("    static const uint32_t FLAGS_RPC_REQUEST_CODE = (1 << 0);\n");
                printer.PrintRaw("    static const uint32_t FLAGS_RPC_LOG_ID = (1 << 1);\n");
            }
            printer.PrintRaw("private:\n");
            printer.PrintRaw("    brpc::ChannelOptions _options;\n");
            printer.PrintRaw("    std::string _service_name;\n");
            if (client_type != "ASyncClient") {
                printer.PrintRaw("    brpc::Controller _controller;\n");
            }
            printer.PrintRaw("    GroupStrategy _group_strategy;\n");
            printer.PrintRaw("    std::string _lb;\n");
            if (client_type == "ASyncClient") {
                printer.PrintRaw("    uint32_t _rpc_flag;\n");
                printer.PrintRaw("    uint64_t _request_code;\n");
                printer.PrintRaw("    uint64_t _log_id;\n");
                printer.PrintRaw("    brpc::CallId _call_id;\n");
            }

            printer.PrintRaw("\n");
            printer.PrintRaw("public:\n");
            printer.Print("    $client_type$(const std::string& service_name);\n", "client_type", client_type);
            printer.Print("    ~$client_type$();\n\n", "client_type", client_type);
            printer.PrintRaw("    void SetGroupStrategy(GroupStrategy group_strategy);\n");
            printer.PrintRaw("    void SetLbStrategy(const std::string& lb);\n");
            printer.PrintRaw("    void SetRequestCode(uint64_t request_code);\n");
            printer.PrintRaw("    void SetConnectTimeoutMs(uint64_t timeout_ms);\n");
            printer.PrintRaw("    void SetTimeoutMs(uint64_t timeout_ms);\n");
            printer.PrintRaw("    void SetMaxRetry(int max_retry);\n");
            printer.PrintRaw("    void SetLogId(uint64_t log_id);\n");

            if (client_type != "ASyncClient") {
                printer.PrintRaw("    bool Failed() { return _controller.Failed(); }\n");
                printer.PrintRaw("    std::string ErrorText() { return _controller.ErrorText(); }\n");
                printer.PrintRaw("    int ErrorCode() { return _controller.ErrorCode(); }\n");
                printer.PrintRaw("    butil::EndPoint RemoteSide() { return _controller.remote_side(); }\n");
                printer.PrintRaw("    butil::EndPoint LocalSide() { return _controller.local_side(); }\n");
                printer.PrintRaw("    int64_t LatencyUs() { return _controller.latency_us(); }\n");
            } else {
                printer.PrintRaw("    void AddRpcFlag(uint32_t flag) { _rpc_flag |= flag; }\n");
                printer.PrintRaw("    bool HasRpcFlag(uint32_t flag) { return _rpc_flag & flag; }\n");
            }

            if (client_type == "SemiSyncClient") {
                printer.PrintRaw("    void Join() { brpc::Join(_controller.call_id()); }\n");
            } else if (client_type == "ASyncClient") {
                printer.PrintRaw("    void Join() { brpc::Join(_call_id); }\n");
            }

            printer.PrintRaw("\n");

            if (client_type == "SyncClient" || client_type == "SemiSyncClient") {
                for (int i = 0; i < file->service_count(); i++) {
                    auto service = file->service(i);
                    for (int j = 0; j < service->method_count(); j++) {
                        auto method = service->method(j);
                        printer.PrintRaw(
                            "    void " + method->name() + "(" + "const " + method->input_type()->name() + "* req, " +
                            method->output_type()->name() + "* res);\n");
                    }
                }
            } else {
                for (int i = 0; i < file->service_count(); i++) {
                    auto service = file->service(i);
                    for (int j = 0; j < service->method_count(); j++) {
                        auto method = service->method(j);
                        printer.PrintRaw(
                            "    void " + method->name() + "(" + "const " + method->input_type()->name() + "* req, " +
                            "std::function<void(bool, " + method->output_type()->name() + "*)> callback);\n");
                    }
                }
            }

            printer.Print("}; // end of $client_type$\n\n", "client_type", client_type);
        }

        // namespace end
        printer.PrintRaw("\n");
        for (auto it = namespaces.rbegin(); it != namespaces.rend(); it++) {
            printer.PrintRaw("} // end of namespace " + *it + "\n");
        }
    }

    void GenerateSourceFile(const FileDescriptor* file, GeneratorContext* generator_context) const {
        std::string file_name = file->name().substr(0, file->name().find('.'));
        std::string source_file_name = file_name + ".client.cpp";
        auto* outstream = generator_context->Open(source_file_name);

        Printer printer(outstream, '$');
        printer.PrintRaw(notic);

        printer.PrintRaw("#include \"" + file_name + ".client.h\"\n");
        printer.PrintRaw("#include \"core/common/channel_manager.h\"\n");
        printer.PrintRaw("#include \"core/common/common_callback.h\"\n");

        // namespace begin
        printer.PrintRaw("\n");
        std::vector<std::string> namespaces;
        split(file->package(), namespaces);
        for (const std::string& ns : namespaces) {
            printer.PrintRaw("namespace " + ns + " { \n");
        }
        printer.PrintRaw("\n\n");
        printer.PrintRaw("using server::common::SharedPtrChannel;\n");
        printer.PrintRaw("using server::common::ChannelManager;\n");
        printer.PrintRaw("using server::common::OnRPCDone;\n\n");

        std::vector<std::string> client_types = {"SyncClient", "ASyncClient", "SemiSyncClient"};
        for (const std::string& client_type : client_types) {
            if (client_type == "ASyncClient") {
                printer.Print(
                    "$client_type$::$client_type$(const std::string& service_name)\n"
                    "    : _service_name(MakeServiceName(service_name))\n"
                    "    , _group_strategy(GroupStrategy::STRATEGY_NORMAL)\n"
                    "    , _lb(\"rr\")\n"
                    "    , _rpc_flag(0) { }\n\n",
                    "client_type",
                    client_type);
            } else {
                printer.Print(
                    "$client_type$::$client_type$(const std::string& service_name)\n"
                    "    : _service_name(MakeServiceName(service_name))\n"
                    "    , _group_strategy(GroupStrategy::STRATEGY_NORMAL)\n"
                    "    , _lb(\"rr\") { }\n\n",
                    "client_type",
                    client_type);
            }
            printer.Print("$client_type$::~$client_type$() {} \n\n", "client_type", client_type);
            printer.Print(
                "void $client_type$::SetGroupStrategy(GroupStrategy "
                "group_strategy) { \n"
                "    _group_strategy = group_strategy; \n"
                "}\n\n",
                "client_type",
                client_type);
            printer.Print(
                "void $client_type$::SetLbStrategy(const std::string& lb) { \n"
                "    _lb = lb; \n"
                "}\n\n",
                "client_type",
                client_type);
            if (client_type == "ASyncClient") {
                printer.Print(
                    "void $client_type$::SetRequestCode(uint64_t request_code) {\n"
                    "    _request_code = request_code; \n"
                    "    AddRpcFlag(FLAGS_RPC_REQUEST_CODE);\n"
                    "}\n\n",
                    "client_type",
                    client_type);
            } else {
                printer.Print(
                    "void $client_type$::SetRequestCode(uint64_t request_code) {\n"
                    "    _controller.set_request_code(request_code); \n"
                    "}\n\n",
                    "client_type",
                    client_type);
            }
            printer.Print(
                "void $client_type$::SetConnectTimeoutMs(uint64_t timeout_ms) {\n"
                "    _options.connect_timeout_ms = timeout_ms; \n"
                "}\n\n",
                "client_type",
                client_type);
            printer.Print(
                "void $client_type$::SetTimeoutMs(uint64_t timeout_ms) {\n"
                "    _options.timeout_ms = timeout_ms; \n"
                "}\n\n",
                "client_type",
                client_type);
            printer.Print(
                "void $client_type$::SetMaxRetry(int max_retry) {\n"
                "    _options.max_retry = max_retry; \n"
                "}\n\n",
                "client_type",
                client_type);
            if (client_type != "ASyncClient") {
                printer.Print(
                    "void $client_type$::SetLogId(uint64_t log_id) {\n"
                    "     _controller.set_log_id(log_id); \n"
                    "}\n\n",
                    "client_type",
                    client_type);
            } else {
                printer.Print(
                    "void $client_type$::SetLogId(uint64_t log_id) {\n"
                    "     _log_id = log_id; \n"
                    "    AddRpcFlag(FLAGS_RPC_LOG_ID);\n"
                    "}\n\n",
                    "client_type",
                    client_type);
            }

            if (client_type == "SyncClient") {
                for (int i = 0; i < file->service_count(); i++) {
                    auto service = file->service(i);
                    for (int j = 0; j < service->method_count(); j++) {
                        auto method = service->method(j);
                        printer.PrintRaw(
                            "void SyncClient::" + method->name() + "(" + "const " + method->input_type()->name() +
                            "* req, " + method->output_type()->name() + "* res) { \n");
                        printer.PrintRaw(
                            "    SharedPtrChannel channel_ptr = \n"
                            "        ChannelManager::GetInstance()->GetChannel(_service_name, "
                            "_group_strategy, _lb, &_options);\n");
                        printer.PrintRaw("    brpc::Channel* channel = channel_ptr.get();\n");
                        printer.PrintRaw("    if (!channel)  {\n");
                        printer.PrintRaw(
                            "        _controller.SetFailed(::brpc::EINTERNAL, \"Failed to "
                            "channel\");\n");
                        printer.PrintRaw("        return;\n    }\n");

                        printer.PrintRaw("    " + service->name() + "_Stub stub(channel);\n");
                        printer.PrintRaw("    stub." + method->name() + "(&_controller, req, res, nullptr);\n");

                        printer.PrintRaw("}\n\n");
                    }
                }
            } else if (client_type == "SemiSyncClient") {
                for (int i = 0; i < file->service_count(); i++) {
                    auto service = file->service(i);
                    for (int j = 0; j < service->method_count(); j++) {
                        auto method = service->method(j);
                        printer.PrintRaw(
                            "void SemiSyncClient::" + method->name() + "(" + "const " + method->input_type()->name() +
                            "* req, " + method->output_type()->name() + "* res) { \n");
                        printer.PrintRaw(
                            "    SharedPtrChannel channel_ptr = \n"
                            "        ChannelManager::GetInstance()->GetChannel(_service_name, "
                            "_group_strategy, _lb, &_options);\n");
                        printer.PrintRaw("    brpc::Channel* channel = channel_ptr.get();\n");
                        printer.PrintRaw("    if (!channel)  {\n");
                        printer.PrintRaw(
                            "        _controller.SetFailed(::brpc::EINTERNAL, \"Failed to "
                            "channel\");\n");
                        printer.PrintRaw("        return;\n    }\n");

                        printer.PrintRaw("    " + service->name() + "_Stub stub(channel);\n");
                        printer.PrintRaw(
                            "    stub." + method->name() + "(&_controller, req, res, brpc::DoNothing());\n");

                        printer.PrintRaw("}\n\n");
                    }
                }
            } else {
                for (int i = 0; i < file->service_count(); i++) {
                    auto service = file->service(i);
                    for (int j = 0; j < service->method_count(); j++) {
                        auto method = service->method(j);
                        printer.PrintRaw(
                            "void ASyncClient::" + method->name() + "(" + "const " + method->input_type()->name() +
                            "* req, " + "std::function<void(bool, " + method->output_type()->name() +
                            "*)> callback) {\n");
                        printer.PrintRaw(
                            "    auto done = new OnRPCDone<" + method->output_type()->name() + ">(callback);\n");
                        printer.PrintRaw(
                            "    SharedPtrChannel channel_ptr = \n"
                            "        ChannelManager::GetInstance()->GetChannel(_service_name, "
                            "_group_strategy, _lb, &_options);\n");
                        printer.PrintRaw("    brpc::Channel* channel = channel_ptr.get();\n");
                        printer.PrintRaw("    if (!channel)  {\n");
                        printer.PrintRaw("        brpc::ClosureGuard done_guard(done);\n");
                        printer.PrintRaw(
                            "        done->cntl.SetFailed(::brpc::EINTERNAL, \"Failed to "
                            "channel\");\n");
                        printer.PrintRaw("        return;\n    }\n");
                        printer.PrintRaw("    if(HasRpcFlag(FLAGS_RPC_REQUEST_CODE)) {\n");
                        printer.PrintRaw("        done->cntl.set_request_code(_request_code);\n");
                        printer.PrintRaw("    }\n");
                        printer.PrintRaw("    if(HasRpcFlag(FLAGS_RPC_LOG_ID)) {\n");
                        printer.PrintRaw("        done->cntl.set_log_id(_log_id);\n");
                        printer.PrintRaw("    }\n");
                        printer.PrintRaw("    " + service->name() + "_Stub stub(channel);\n");
                        printer.PrintRaw("    _call_id == done->cntl.call_id();\n");
                        printer.PrintRaw("    stub." + method->name() + "(&done->cntl, req, &done->response, done);\n");

                        printer.PrintRaw("}\n\n");
                    }
                }
            }
            printer.PrintRaw("\n");
        }

        // namespace end
        for (auto it = namespaces.rbegin(); it != namespaces.rend(); it++) {
            printer.PrintRaw("} // end of namespace " + *it + "\n");
        }
    }

private:
    const std::string notic =
        "/************************************************************************\n\
****  This code is auto generated by plugin, please do not modify!   ****\n\
****          author: meicorl      email: 756621494@qq.com           ****\n\
************************************************************************/\n\n";
};

int main(int argc, char** argv) {
    CodeXXGenerator generator;
    return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}