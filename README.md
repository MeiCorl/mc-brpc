# 简介
### &emsp;&emsp; [mc-brpc](https://github.com/MeiCorl/mc-brpc)是基于[百度brpc框架](https://brpc.apache.org/)快速开发brpc服务的脚手架框架，目的是简化构建brpc服务的构建流程以及发起brpc请求的流程，减少业务代码开发量。其次，mc-brpc在brpc的基础上扩展增加了以下功能以更好的支持rpc服务开发：
* **服务注册**：brpc对于rpc服务中一些功能并未提供具体实现，而是需要用户自己去扩展实现。如服务注册，因为不同用户使用的服务注册中心不一样，有Zookeeper、Eureka、Nacos、Consul、Etcd、甚至自研的注册中心等，不同注册中心注册和发现流程可能不一样，不太好统一，因此只好交由用户根据去实现服务注册这部分；而对于服务发现这部分，brpc默认支持基于文件、dns、bns、http、consul、nacos等的服务发现，并支持用户扩展NamingService实现自定义的服务发现。mc-brpc在brpc的基础上实现了服务自动注册到etcd，并提供了NamingService的扩展(<font color=#00ffff>McNamingService</font>)用以支持自定义的服务发现(**mc:\/\/service_name**)

* **名字服务代理**：mc-brpc服务启动会自动注册到etcd，但却不直接从etcd直接做服务发现，而是提供了一个<font color=#00ffff>brpc_name_agent</font>基础服务作为名字服务代理，它负责从etcd实时更新服务信息，并为mc-brpc提供服务发现，主要是为了支持在服务跨机房甚至跨大区部署时，brpc请求能支持按指定大区和机房进行路由，此外name_agent还将服务信息dump出来作为promethus监控的targets，以及后续拟支持户端主动容灾功(暂未实现)等

* **日志异步刷盘**：brpc提供了日志刷盘抽象工具类LogSink，并提供了一个默认的实现DefaultLogSink，但是DefaultLogSink写日志是同步写，且每写一条日志都会写磁盘，性能较差，在日志量大以及对性能要求较高的场景下很难使用，而百度内部使用的ComlogSink实现似乎未开源(看代码没找到)，因此mc-brpc自己实现了个<font color=#00ffff>AsyncLogSink</font>先将日志写缓冲区再批量刷盘(也得感谢brpc插件式的设计，极大的方便了用户自己做功能扩展)，AsyncLogSink写日志先写缓冲区，再由后台线程每秒批量刷盘，性能相比DefaultLogSink由10倍以上提升，但是在服务崩溃的情况下可能会丢失最近1s内的日志

* **日志自动滚动归档**：<font color=#00ffff>LogRotateWatcher</font>及<font color=#00ffff>LogArchiveWorker</font>每小时对日志进行滚动压缩归档，方便日志查询，并删除一段时间(默认1个月，可以公共通过log配置的remain_days属性指定)以前的日志，避免磁盘写满

* **自动生成rpc客户端代码**：当通过某个服务通过proto文件定义好接口后，其它服务若想调用该服务的接口，只需要在CMakeLists.txt(参考services/brpc_test/CmakeLists.txt)中调用<font color=#ffff00>auto_gen_client_code</font>即可为指定proto生成对应的同步客户端(<font color=#00ffff>SyncClient</font>)、半同步客户端(<font color=#00ffff>SemiSyncClient</font>)及异步客户端(<font color=#00ffff>ASyncClient</font>)，简化发起brpc调用的流程。其原理是通过一个mc-brpc提供的protobuf插件<font color=#00ff00>codexx</font>(core/plugins/codexx)解析对应proto文件然后生成相应客户端代码

* **统一全局配置**：mc-brpc服务启动会自动解析gflags(如果有../conf/gflags.conf配置文件)；并自动解析服务配置(../conf/server.conf)，其中包含服务名、名字服务地址、大区id、机房id、DB配置、Redis配置、日志相关配置等，并自动根据服务配置对相关组件进行初始化操作(如：根据DB配置创建DB连接池，根据Redis配置初始化Redis客户端代理实例等)

* **DB连接管理**：core/mysql下基于[libmysqlclient](https://dev.mysql.com/downloads/c-api/)封装实现了<font color=#00ffff>MysqlConn</font>并提供了对应连接池实现<font color=#00ffff>DBPool</font>，mc-brpc服务启动时会根据配置为每个DB实例初始化一个连接池对象并注册到一个全局map中，使用时通过<font color=#00ffff>DBManager</font>从对应实例的连接池获取一个连接进行DB操作。DBPool支持设置连接池最小空闲连接数、最大活跃连接数、获取链接超时时间、连接空闲超时时间（长期空闲超时的连接会被自动释放）等

* **Redis连接管理**：core/redis下基于[redis-plus-plus](https://github.com/sewenew/redis-plus-plus)封装实现了客户端代理类<font color=#00ffff>RedisWrapper</font>(主要目的在于对业务代码屏蔽redis++在操作单实例redis、哨兵模式redis以及集群模式redis的差异)。同DB连接管理一样，mc-brpc服务启动时会根据配置为每个Redis(单实例/Sentine/Cluster)初始化一个RedisWrapper对象并注册到一个全局map中，使用时通过<font color=#00ffff>RedisManager</font>从对应redis(集群)获取一个连接进行操作。这里redis连接池的管理就不需要做额外实现了，redis++会为每个redis实例创建一个连接池并管理连接(如果是集群则为每个master节点创建一个连接池)

# 项目结构
├── brpc   &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp; # brpc源码(做了少量忽略不计的修改，功力有限^-^)  
├── core   &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp; # 新增的拓展及其他功能都在这个目录下     
│   ├── common         &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp; # 一些常用公共代码实现   
│   ├── config         &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp; # 服务配置类代码    
│   ├── extensions     &emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp; # 对brpc中一些功能的扩展  
│   ├── log            &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;&nbsp; # 日志组件扩展  
│   ├── mysql          &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp; # MySQL连接池及工具类实现  
│   ├── plugins        &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp; # 插件工具，目前仅包含codexx的实现，用户根据proto文件自动生成客户端代码  
│   ├── redis          &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp; # Redis客户端代理及工具类实现  
│   ├── server         &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp; # 对brpc server的一些封装   
│   └── utils          &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp; # 一些工具类实现(感觉可以放到common目录)   
└── services           &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp; # 基于mc-brpc开发的brpc服务示例  
│       ├── brpc_name_agent    &emsp;&emsp;&nbsp; # 名字服务代理(作为一个基础服务，需要打包部署到每个容器镜像中，为该容器内的其它服务提供服务发现)  
│       ├── brpc_test  &emsp;&emsp;&emsp;&emsp;&emsp;&emsp; # 示例服务  

# 功能介绍
## 1. MCServer
&emsp;&emsp; MCServer是对brpc::Server的封装，其目的是简化brpc::Server的构建流程以及增加自动解析gflags配置文件、自动解析服务配置文件server.conf，并根据配置初始化日志相关组件、创建DB连接池及Redis连接池以及向服务中心注册服务信息等。以下分别是基于原始brpc和mc-brpc构建brpc服务的代码，从`main`函数里可以看出基于mc-brpc的代码实现更为简洁(但启动过程一致，部分过程封装在MCServer内部实现了)。  
**brpc:**
```c++ 
int main(int argc, char* argv[]) {
    // Parse gflags. We recommend you to use gflags as well.
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);  

    // Generally you only need one Server.
    brpc::Server server; 

    // Instance of your service.
    test::ServiceImpl service;

    // Add the service into server. Notice the second parameter, because the
    // service is put on stack, we don't want server to delete it, otherwise
    // use brpc::SERVER_OWNS_SERVICE.
    if (server.AddService(&service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service";
        return -1;
    }

    butil::EndPoint point;
    if (!FLAGS_listen_addr.empty()) {
        if (butil::str2endpoint(FLAGS_listen_addr.c_str(), &point) < 0) {
            LOG(ERROR) << "Invalid listen address:" << FLAGS_listen_addr;
            return -1;
        }
    } else {
        point = butil::EndPoint(butil::IP_ANY, FLAGS_port);
    }

    brpc::ServerOptions options;
    options.idle_timeout_sec = FLAGS_idle_timeout_s;
    // Start the server.
    if (server.Start(point, &options) != 0) {  
        LOG(ERROR) << "Fail to start EchoServer";
        return -1;
    }

    // Wait until Ctrl-C is pressed, then Stop() and Join() the server.
    server.RunUntilAskedToQuit();  
    return 0;
}
```

**mc_brpc:**
```c++
int main(int argc, char* argv[]) {
    // Instance of MCServer.
    server::MCServer server(argc, argv);

    // Instance of your service.
    test::ServiceImpl service;

    // Add the service into server.
    server.AddService(&service);

    // Start the server.
    server.Start();

    return 0;
}
```
### 实例化过程
```c++
MCServer::MCServer(int argc, char* argv[]) {
    // 解析gflags
    if (FILE* file = fopen(FLAGS_gflags_path.c_str(), "r")) {
        google::SetCommandLineOption("flagfile", FLAGS_gflags_path.c_str());
        fclose(file);
    }

    // 初始化日志（会额外触发server.conf全局配置解析)
    LoggingInit(argv);

    // 注册名字服务(McNamingService)
    RegisterNamingService();

#ifdef USE_MYSQL
    // init db if necessary
    DBManager::get()->Init();
#endif

#ifdef USE_REDIS
    // init redis cluster if necessary
    RedisManager::get()->Init();
#endif
}
```
1. <font color=#00ffff>MCServer</font>包含了brpc::Server，因此MCServer实例化首先也会触发brpc::Server的实例化。  
2. 判断是否有gflags.conf配置文件(../conf/gflags.conf)，如果有则自动触发gflags解析。可以在这里通过gflags配置控制一些brpc功能的开关(如优雅退出、rpcz等，参考services/brpc_test/conf/gflags.conf)，以及设置自定义gflags参数。
3. 解析服务配置文件(../conf/server.conf)，里面包含注册中心地址、服务名、所在大区id、机房id、DB配置、Redis配置、日志配置等(参考services/brpc_test/conf/server.conf)。
```c++
using namespace server::config;

DEFINE_string(config_file, "../conf/server.conf", "Server config file");

ServerConfig::ServerConfig(/* args */) {
    int fd = ::open(FLAGS_config_file.c_str(), O_RDONLY);
    if (fd == -1) {
        LOG(ERROR) << "[!] Can not open server.conf";
        return;
    }
    butil::fd_guard fdGuard(fd);

    google::protobuf::io::FileInputStream* input = new google::protobuf::io::FileInputStream(fd);
    bool status = google::protobuf::TextFormat::Parse(input, &_config);
    if (!status) {
        LOG(ERROR) << "[!] Can not parse server.conf";
        return;
    }
    delete input;
    input = nullptr;
}
```
4. 根据3中服务日志配置，初始化日志输出文件路径并创建日志文件监听线程和日志归档线程(二者配合实现日志分时归档压缩)，以及判断是否需要使用异步日志写入AsyncLogSink(CMakeLists.txt中add_server_source添加<font color=#ffff00>ASYNCLOG</font>选项, 如`add_server_source(SERVER_SRCS ASYNCLOG)`, 这样会触发<font color=#ffff00>USE_ASYNC_LOGSINK</font>宏定义)。
5. 根据3中服务配置，初始化DB连接池(CMakeLists.txt中add_server_source添加<font color=#ffff00>USE_MYSQL</font>选项，触发定义<font color=#ffff00>USE_MYSQL</font>宏定义以及MySQL相关源文件及依赖库引入)及Redis连接池(CMakeLists.txt中add_server_source添加<font color=#ffff00>USE_REDIS</font>选项，触发<font color=#ffff00>USE_REDIS</font>宏定义及相关源文件和依赖库引入)。
6. 注册自定义的名字服务<font color=#00ffff>McNamingService</font>用以支持**mc:\/\/** 前缀的服务发现

### 启动过程
1. 首先指定服务监听地址。可以通过gflags<*listen_addr*>指定ip:port或者unix_socket地址, 否则使用当前ip地址+随机端口。调brpc::Start启动server
2. 将服务信息(ip:port、服务名、大区id、机房id)注册到服务中心。这里使用etcd作为服务中心，并通过etcd的租约续期功能为注册信息续期。
3. 调brpc::RunUntilAskedToQuit死循环直到进程被要求退出。

### 退出过程
退出过程相对简单。销毁日志监听线程、日志归档线程，从服务中心取消注册并停止etcd租约续期等。

## 2. 服务注册及发现
### 服务注册
MCSever启动会时，会从服务配置里获取注册中心地址、当前服务名、大区id、及机房id等，并自动向etcd注册服务信息，这里使用的了[etcd-cpp-apiv3](https://github.com/etcd-cpp-apiv3/etcd-cpp-apiv3)。注册key为"*服务名:实例id*"(这里实例id设计比较简单，直接用实例信息序列化后的字符串计算hash code，暂时没有实际用处), value为实例信息(server::config::InstanceInfo, 参考core/config/server_config.proto)序列化后字符串。
```c++
bool MCServer::RegisterService() {
    ServerConfig* config = utils::Singleton<ServerConfig>::get();
    etcd::Client etcd(config->GetNsUrl());
    server::config::InstanceInfo instance;
    instance.set_region_id(config->GetSelfRegionId());
    instance.set_group_id(config->GetSelfGroupId());
    instance.set_endpoint(butil::endpoint2str(_server.listen_address()).c_str());
    etcd::Response resp = etcd.leasegrant(REGISTER_TTL).get();
    if (resp.error_code() != 0) {
        LOG(ERROR) << "[!] etcd failed, err_code: " << resp.error_code() << ", err_msg:" << resp.error_message();
        return false;
    }
    _etcd_lease_id = resp.value().lease();
    etcd::Response response =
        etcd.set(BuildServiceName(config->GetSelfName(), instance), instance.SerializeAsString(), _etcd_lease_id).get();
    if (response.error_code() != 0) {
        LOG(ERROR) << "[!] Fail to register service, err_code: " << response.error_code()
                   << ", err_msg:" << response.error_message();
        return false;
    }

    std::function<void(std::exception_ptr)> handler = [this](std::exception_ptr eptr) {
        try {
            if (eptr) {
                std::rethrow_exception(eptr);
            }
        } catch (const std::runtime_error& e) {
            LOG(FATAL) << "[!] Etcd connection failure: " << e.what();
        } catch (const std::out_of_range& e) {
            LOG(FATAL) << "[!] Etcd lease expire: " << e.what();
        }

        // KeepAlive续约有时因为一些原因lease已经过期则会失败，或者etcd重启也会失败，需要发起重新注册
        // 注意需要新起一个线程发起重新注册，否则会出现死锁（不新起线程的话，那重新注册的逻辑是运行在当前
        // handler所在线程的，而旧的KeepAlive对象析构又需要等待当前handler所在线程结束，具体请看KeepAlive源码）
        auto fn = std::bind(&MCServer::TryRegisterAgain, this);
        std::thread(fn).detach();
    };

    // 注册信息到期前5s进行续约，避免租约到期后续约失败
    _keep_live_ptr.reset(new etcd::KeepAlive(config->GetNsUrl(), handler, REGISTER_TTL - 5, _etcd_lease_id));
    LOG(INFO) << "Service register succ. instance: {" << instance.ShortDebugString()
              << "}, lease_id:" << _etcd_lease_id;
    return true;
}

/**
 * 用于在一些异常情况下(如etcd重启或者旧的lease过期导致keepalive对象失败退出)重新发起注册
 **/
void MCServer::TryRegisterAgain() {
    do {
        LOG(INFO) << "Try register again.";
        bool is_succ = RegisterService();
        if (is_succ) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    } while (true);
}

std::string MCServer::BuildServiceName(
    const std::string& original_service_name,
    const server::config::InstanceInfo& instance) {
    std::hash<std::string> hasher;
    return original_service_name + ":" + std::to_string(hasher(instance.SerializeAsString()));
}
```

### brpc_name_agent
mc-brpc服务并不直接从服务中心做服务发现，而是引入了一个基础服务brpc_name_agent作为名字服务代理。brpc_name_agent本身也是基于mc-brpc创建，它从etcd订阅了key事件通知，当有新的服务实例注册或者取消注册时，brpc_name_agent将对应服务实例信息添加至本地内存或者从本地内存移除(典型读多写少场景，使用<font color=#00ffff>DoublyBufferedData</font>存储数据)。因此，每个name_agent实例都包含全部注册到etcd的服务实例信息。默认情况下，brpc_name_agent使用unix域套接字进行通信(unix:/var/brpc_name_agent.sock)，因此它不需要注册到etcd中，name_agent进程需要部署到每个服务器上供该服务器上的其他mc-brpc服务进程做服务发现。
```c++
void AgentServiceImpl::WatcherCallback(etcd::Response response) {
    for (const etcd::Event& ev : response.events()) {
        string key          = ev.kv().key();
        string service_name = key.substr(0, key.find(":"));

        if (ev.event_type() == etcd::Event::EventType::PUT) {
            InstanceInfo info;
            if (!info.ParseFromString(ev.kv().as_string())) {
                LOG(ERROR) << "[!] Invalid service info, key:" << ev.kv().key()
                           << " , value:" << ev.kv().as_string();
                continue;
            }

            AddServer(service_name, info);
        } else if (ev.event_type() == etcd::Event::EventType::DELETE_) {
            InstanceInfo info;
            if (!info.ParseFromString(ev.prev_kv().as_string())) {
                LOG(ERROR) << "[!] Invalid service info: " << ev.prev_kv().as_string();
                continue;
            }

            RemoveServer(service_name, info);
        } else {
            LOG(ERROR) << "[!] Invalid event type:" << static_cast<int>(ev.event_type())
                       << " key:" << ev.kv().key() << " value:" << ev.kv().as_string();
        }
    }
}
```

引入brpc_name_agent的好处是:
1. 服务跨机房甚至跨区部署时，提供更丰富的请求路由策略。在name_agent内部分区域和机房存储服务实例信息，便于支持上游服务发现时按指定大区和机房做请求路由(参考AgentServiceImpl::GetServers)。目前支持<font color=#00ff00>STRATEGY_NORMAL</font>(先本机房IP列表路由，本机房无实例再本大区路由，无实例则路由失败)，<font color=#00ff00>STRATEGY_GROUPS_ONE_REGION</font>(直接本大区IP列表路由，无实例则路由失败)、<font color=#00ff00>STRATEGY_SELF_GROUP</font>(只在本机房路由，无实例则路由失败)、<font color=#00ff00>STRATEGY_CHASH_GROUPS</font>(先对本大区的所有机房做一致性HASH决定所返回的IP列表的机房, 再返回机房的实例IP列表路由)
2. 客户端主动容灾(暂未实现)。brpc的主要client容灾手段为心跳检查，即一个后端连接如果断掉且重连失败，会从名字里摘掉。心跳检查可以解决一部分问题，但是有其局限性。当后端某个ip cpu负载高，网络抖动，丢包率上升等情况下，一般心跳检查是能通过的，但是此时我们判断该ip是处于一种异常状态，需要降低访问权重，来进行调节。为此，需要收集并统计rpc调用，生成容灾策略。
3. 将服务实例信息dump到统一文件，供promethus做监控目标并从对应目标拉去metrics信息，如当前有1个brpc_test服务实例，2个brpc_test1实例，3个brpc_test2实例，name_agent进程将生成如下监控目标文件供prometheus监控。
```json
[
    {
        "targets":[
            "172.17.0.2:38003",
            "172.17.0.2:35385",
            "172.17.0.2:40273"
        ],
        "labels":{
            "service_name":"brpc_test2",
            "region_id":"1",
            "group_id":"1002"
        }
    },
    {
        "targets":[
            "172.17.0.2:41349",
            "172.17.0.2:39239"
        ],
        "labels":{
            "service_name":"brpc_test1",
            "region_id":"1",
            "group_id":"1001"
        }
    },
    {
        "targets":[
            "172.17.0.2:40253"
        ],
        "labels":{
            "service_name":"brpc_test",
            "region_id":"1",
            "group_id":"1001"
        }
    }
]
```
假设上述文件路径为：/etc/prometheus/instance.json，则为prometheus配置以下抓取任务即可实现对每个mc-brpc服务实例metrics监控：
```yaml
scrape_configs:
    # The job name is added as a label `job=<job_name>` to any timeseries scraped from this config.
    # metrics_path defaults to '/metrics'
    # scheme defaults to 'http'.
  - job_name: "brpc_services"
    metrics_path: "brpc_metrics"
    file_sd_configs:
    - refresh_interval: 10s
      files:
        - /etc/prometheus/instance.json
```
*注意: metrics_path需要指定为brpc_metrics, brpc服务默认metrics到处路径为/brpc_metrics*, 以下是通过[grafana](https://grafana.com/)对prometheus采集的服务指标进行可视化的示例：
![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d818623c7d1840c383e10949518920aa~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=2560&h=1239&s=261197&e=png&b=181b1f)

### 服务发现
brpc支持用户扩展NamingService实现自定义的服务发现，mc-brpc在此基础上扩展实现了<font color=#00ffff>McNamingService</font>, 它继承实现了<font color=#00ffff>brpc::PeriodicNamingService</font>，周期性的(默认每个5秒)从<font color=#00ffff>brpc_name_agent</font>按**指定大区机房策略**更新服务实例节点信息。
```c++
int McNamingService::GetServers(const char* service_name, std::vector<ServerNode>* servers) {
    if (!_name_agent_inited) {
        return -1;
    }

    std::string s(service_name);
    size_t p                      = s.find(':');
    std::string real_service_name = s.substr(0, p);
    uint32_t group_strategy       = std::strtoul(s.substr(p + 1).c_str(), nullptr, 10);

    brpc::Controller cntl;
    name_agent::GetServersReq req;
    name_agent::GetServersRes res;
    req.set_seq_id(butil::gettimeofday_ms());
    req.set_service_name(real_service_name);
    req.set_group_strategy(group_strategy);
    _name_agent_stub->GetServers(&cntl, &req, &res, nullptr);
    if (!cntl.Failed()) {
        for (const std::string& endpoint : res.endpoints()) {
            butil::EndPoint ep;
            butil::str2endpoint(endpoint.c_str(), &ep);
            brpc::ServerNode node(ep);
            servers->emplace_back(node);
        }
        return 0;
    } else {
        LOG(ERROR) << "[!] NameAgent GetServers Error: " << cntl.ErrorText();
        return -1;
    }
}
```
brpc访问下游是通过channel发起请求，channel可以看做是带有lb策略的(rr、c_murmurhash等负载均衡策略)面向一台或一组服务器的交互通道, 它可以被所以线程共用，不需要反复创建，mc-brpc在此基础上提供了<font color=#00ffff>ChannelManager</font>, 它用于按照指定机房策略和lb策略获取下游的channel并缓存，上游可以通过`SharedPtrChannel channel_ptr = 
        SingletonChannel::get()->GetChannel(_service_name, _group_strategy, _lb, &_options);`获取某个下游服务的channel并发起访问，其中`using SingletonChannel = server::utils::Singleton<ChannelManager>;`  但更多情况下我们不想使用这种方式，因为这样发起brpc请求的业务代码比较繁琐，首先要获取对应服务的channel，然后要用该channel初始化一个stub对象，同时还行要声明一个<font color=#00ffff>brpc::Controller</font>对象用于存储rpc请求元数据，最后在通过该stub对象发起rpc调用。这个过程对于每个brpc请求都是一样的，我们可以提供一个统一的实现，不用在业务层去写过多代码，也就是我们后续要提到的自动生成客户端代码部分，为需要调用的服务生成一个<font color=#00ffff>Client</font>对象，直接通过<font color=#00ffff>Client</font>对象就可以发起rpc调用。  

## 3. 自动生成rpc客户端代码
&emsp;&emsp; 原生brpc发起客户端调用示例如下：
```c++
    // 1. Initialize the channel
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.protocol = FLAGS_protocol;
    options.connection_type = FLAGS_connection_type;
    options.timeout_ms = FLAGS_timeout_ms；
    options.max_retry = FLAGS_max_retry;
    if (channel.Init(FLAGS_server.c_str(), FLAGS_load_balancer.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }

    // 2. Normally, you should not call a Channel directly, but instead construct a stub Service wrapping it. stub can be shared by all threads as well.
    example::EchoService_Stub stub(&channel);

    // 3. create a controller
    brpc::Controller cntl;
    cntl.set_log_id(log_id++);
    /* Set attachment which is wired to network directly instead of   being serialized into protobuf messages. */
    cntl.request_attachment().append(FLAGS_attachment); 

    // 4. send rpc
    stub.Echo(&cntl, &request, &response, NULL);
    if (!cntl.Failed()) {
        LOG(INFO) << "Received response from " << cntl.remote_side()
            << " to " << cntl.local_side()
            << ": " << response.message() << " (attached="
            << cntl.response_attachment() << ")"
            << " latency=" << cntl.latency_us() << "us";
    } else {
        LOG(WARNING) << cntl.ErrorText();
    }
```
上述过程大致分为4步：  
1. 创建一个面向目标服务的channel实例
2. 用channel构造一个stub对象
3. 创建一个controller对象用户保存rpc过程中的一些控制数据等
4. 通过stub对象向目标服务发起rpc调用  

通过上述同步调用示例，可以看出发起rpc调用的流程较长，如果需要向多个不同服务的不同接口发起rpc调用需要编写大量重复类似的代码。因此，mc-brpc对此过程进行了简化，我们提供了个插件<font color=#ffff0>codexx</font>（代码实现：core/extensions/codexx.cpp），它通过对目标服务的proto文件进行解析，为每个目标服务生成对应的同步客户端(<font color=#00ffff>SyncClient</font>)、半同步客户端(<font color=#00ffff>SemiSyncClient</font>)及异步客户端(<font color=#00ffff>ASyncClient</font>)，简化发起brpc调用的流程。只需要在CMakeList.txt中对添加下面两行即可：
```cmake
file(GLOB TEST_PROTO "${CMAKE_CURRENT_SOURCE_DIR}/proto/test.proto")
auto_gen_client_code(${CMAKE_CURRENT_SOURCE_DIR}/client ${CMAKE_CURRENT_SOURCE_DIR}/proto/ ${TEST_PROTO})
```
其中<font color=#ffff00>auto_gen_client_code</font>函数定义域global.cmake中(调用codexx对目标proto进行解析并生成对应的客户端类)：
```cmake
function (auto_gen_client_code out_path proto_path proto_file)
    file(MAKE_DIRECTORY ${out_path})

    foreach(p ${proto_path})
        set(proto_path_list ${proto_path_list} "--proto_path=${p}")
    endforeach()
    message(STATUS "proto_path_list: ${proto_path_list}")

    foreach(f ${proto_file})
        execute_process(COMMAND /usr/bin/protoc --plugin=protoc-gen-cxx=${ROOT_PATH}/core/plugins/codexx --cxx_out=${out_path} ${proto_path_list} ${f})
    endforeach()
endfunction()
```
服务编译后，会在client目录下生成对应的client源文件xxx.client.h和xxx.client.cpp。下面给出部分services/brpc_test/proto/test.proto编译后自动生成的客户端代码:  
test.client.h
```c++
// 同步客户端
class SyncClient {
private:
    brpc::ChannelOptions _options;
    std::string _service_name;
    brpc::Controller _controller;
    GroupStrategy _group_strategy;
    std::string _lb;

public:
    SyncClient(const std::string& service_name);
    ~SyncClient();

    void SetGroupStrategy(GroupStrategy group_strategy);
    void SetLbStrategy(const std::string& lb);
    void SetRequestCode(uint64_t request_code);
    void SetConnectTimeoutMs(uint64_t timeout_ms);
    void SetTimeoutMs(uint64_t timeout_ms);
    void SetMaxRetry(int max_retry);
    void SetLogId(uint64_t log_id);
    bool Failed() { return _controller.Failed(); }
    std::string ErrorText() { return _controller.ErrorText(); }
    int ErrorCode() { return _controller.ErrorCode(); }
    butil::EndPoint RemoteSide() { return _controller.remote_side(); }
    butil::EndPoint LocalSide() { return _controller.local_side(); }
    int64_t LatencyUs() { return _controller.latency_us(); }

    /*  Begin your rpc interfaces  */
    void UpdateUserInfo(const UpdateUserInfoReq* req, UpdateUserInfoRes* res);
    void Test(const TestReq* req, TestRes* res);
};

// 异步客户端
class ASyncClient { /* ... */ };

// 半同步客户端
class SemiSyncClient { /* ... */ };
```

test.client.cpp
```c++
// 同步调用
void SyncClient::Test(const TestReq* req, TestRes* res) { 
    SharedPtrChannel channel_ptr = 
        SingletonChannel::get()->GetChannel(_service_name, _group_strategy, _lb, &_options);
    brpc::Channel* channel = channel_ptr.get();
    if (!channel)  {
        _controller.SetFailed(::brpc::EINTERNAL, "Failed to channel");
        return;
    }
    TestService_Stub stub(channel);
    stub.Test(&_controller, req, res, nullptr);
}

// 异步调用
void ASyncClient::Test(const TestReq* req, std::function<void(bool, TestRes*)> callback) {
    auto done = new OnRPCDone<TestRes>(callback);
    SharedPtrChannel channel_ptr = 
        SingletonChannel::get()->GetChannel(_service_name, _group_strategy, _lb, &_options);
    brpc::Channel* channel = channel_ptr.get();
    if (!channel)  {
        brpc::ClosureGuard done_guard(done);
        done->cntl.SetFailed(::brpc::EINTERNAL, "Failed to channel");
        return;
    }
    if(HasRpcFlag(FLAGS_RPC_REQUEST_CODE)) {
        done->cntl.set_request_code(_request_code);
    }
    if(HasRpcFlag(FLAGS_RPC_LOG_ID)) {
        done->cntl.set_log_id(_log_id);
    }
    TestService_Stub stub(channel);
    _call_id == done->cntl.call_id();
    stub.Test(&done->cntl, req, &done->response, done);
}

// 半同步调用
void SemiSyncClient::Test(const TestReq* req, TestRes* res) { 
    SharedPtrChannel channel_ptr = 
        SingletonChannel::get()->GetChannel(_service_name, _group_strategy, _lb, &_options);
    brpc::Channel* channel = channel_ptr.get();
    if (!channel)  {
        _controller.SetFailed(::brpc::EINTERNAL, "Failed to channel");
        return;
    }
    TestService_Stub stub(channel);
    stub.Test(&_controller, req, res, brpc::DoNothing());
}
```
其中`SingletonChannel::get()`用户获取一个全局单例的<font color=#00ffff>ChannelManager</font>对象，再通过它的`ChannelManager::GetChannel`方法按指定的大区机房策略以及负载均衡策略获取目标服务的`brpc::Channel`指针:
```c++
SharedPtrChannel ChannelManager::GetChannel(const std::string& service_name,
                                            GroupStrategy group_strategy,
                                            const std::string& lb,
                                            brpc::ChannelOptions* options) {
    /**
     * service_name(for example: "mc://brpc_name_agent")
     * real_service_name(for example: "mc://brpc_name_agent:0")
     * service_name_with_lb(for example: "mc://brpc_name_agent:0:rr")
    **/

    std::string real_service_name    = service_name + ":" + std::to_string(group_strategy);
    std::string service_name_with_lb = real_service_name + ":" + lb;

    BAIDU_SCOPED_LOCK(_mutex);
    auto it = _server_channels.find(service_name_with_lb);
    if (it != _server_channels.end()) {
        return it->second;
    }

    SharedPtrChannel channel(new brpc::Channel);
    if (channel->Init(real_service_name.c_str(), lb.c_str(), options) != 0) {
        LOG(ERROR) << "[!] Failed to new Channel, service_name_with_lb:" << service_name_with_lb;
        return nullptr;
    }

    _server_channels[service_name_with_lb] = channel;
    return channel;
}
```

业务代码中引入对应client头文件便可以通过Client对象直接发起rpc调用，如下：  
**同步调用:**
```c++
test::SyncClient client("brpc_test");
test::TestReq req;
test::TestRes res;
req.set_seq_id(request->seq_id());
req.set_msg("hello world");
client.Test(&req, &res);
if (client.Failed()) {
    LOG(ERROR) << "[!] RpcError:" << client.ErrorText();
    response->set_res_code(ServerError);
    return;
}
```
**异步调用:**
```c++
test::ASyncClient client("brpc_test");
test::TestReq req;
req.set_seq_id(request->seq_id());
req.set_msg("hello world");
client.Test(&req, [](bool isFailed, TestRes* res) {
    if(isFailed) {
        LOG(ERROR) << "[!] Rpc Failed.";
        return;
    }
    // todo: do something
});
```

**半同步调用:**
```c++
test::SemiSyncClient client("brpc_test");
test::TestReq req;
req.set_seq_id(request->seq_id());
req.set_msg("hello world");
client.Test(&req, &res);
client.Join();
```
对比可发现，不同类型rpc调用客户端代码相比原始brpc调用方式要简洁很多。示例里面只给出了最简单的用法，自动生成的Client类可以像brpc::Controller一样设置rpc连接超时时间、rpc超时时间、重试次数、LogId、获取rpc延迟时间等，以及设置rpc请求机房路由策略和负载均衡策略等, 如：
```c++
// 按request_code对同大区brpc_test服务实例hash请求，连接超时时间1s，rpc超时时间3s，最大重试次数3
test::SyncClient client("brpc_test");
client.SetConnectTimeoutMs(1000);
client.SetTimeoutMs(3000);
client.SetMaxRetry(3);
client.SetGroupStrategy(GroupStrategy::STRATEGY_GROUPS_ONE_REGION);
client.SetLbStrategy("c_murmurhash");
client.SetRequestCode(1234567890);
client.Test(&req, &res);
```


## 4. 滚动异步日志
&emsp;&emsp; brpc写日志是通过<font color=#ffff00>LOG</font>宏进行流式写入的(如：`LOG(INFO) << "abc" << 123;`)，<font color=#ffff00>LOG</font>宏定义涉及到的其它宏定义如下：
```c++
// A few definitions of macros that don't generate much code. These are used
// by LOG() and LOG_IF, etc. Since these are used all over our code, it's
// better to have compact code for these operations.
#define BAIDU_COMPACT_LOG_EX(severity, ClassName, ...)  \
    ::logging::ClassName(__FILE__, __LINE__,  __func__, \
    ::logging::BLOG_##severity, ##__VA_ARGS__)

#define BAIDU_COMPACK_LOG(severity)             \
    BAIDU_COMPACT_LOG_EX(severity, LogMessage)

// Helper macro which avoids evaluating the arguments to a stream if
// the condition doesn't hold.
#define BAIDU_LAZY_STREAM(stream, condition)                            \
    !(condition) ? (void) 0 : ::logging::LogMessageVoidify() & (stream)

// We use the preprocessor's merging operator, "##", so that, e.g.,
// LOG(INFO) becomes the token BAIDU_COMPACK_LOG(INFO).  There's some funny
// subtle difference between ostream member streaming functions (e.g.,
// ostream::operator<<(int) and ostream non-member streaming functions
// (e.g., ::operator<<(ostream&, string&): it turns out that it's
// impossible to stream something like a string directly to an unnamed
// ostream. We employ a neat hack by calling the stream() member
// function of LogMessage which seems to avoid the problem.
#define LOG_STREAM(severity) BAIDU_COMPACK_LOG(severity).stream()

#define LOG(severity)                                                   \
    BAIDU_LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity))
#define LOG_IF(severity, condition)                                     \
    BAIDU_LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity) && (condition))
```
分析源码后可以知道，brpc通过LOG打印日志的过程如下：  
1. **获取当前线程里的LogStream对象**：通过`LOG(security_level)`宏展开后会生成一个<font color=#00ffff>LogMessage</font>临时对象, 并通过该临时对象的`stream()`方法返回一个<font color=#00ffff>LogStream</font>类型的成员指针指向对象的引用，而成员指针在LogMessage对象构造的时候被初始化为指向一个ThreadLocal的<font color=#00ffff>LogStream</font>对象，因此即相当于获取当了当前线程的<font color=#00ffff>LogStream</font>对象。
2. **将日志写入到LogStream对象**：<font color=#00ffff>LogStream</font>继承了std::ostream，因此可以通过c++流插入运算符(`<<`)直接向其中写入日志数据，这里写入的时候会先往<font color=#00ffff>LogStream</font>中写入一些前缀信息，如日志级别、打印时间、文件名及代码行数、函数名(mc-brpc新增了trace_id，便于服务调用全链路追踪)等，之后再是写入用户想要写入的日志内容
3. **日志持久化到文件**：<font color=#00ffff>LogStream</font>只充当了个缓冲区，还需要将缓冲区的内容写入到文件进行持久化，brpc将这部分功能交由<font color=#00ffff>LogSink</font>来完成(用户可以继承LogSink类并重写OnLogMessage方法来实现定制化的写入)。具体实现为当LOG所在代码行结束时则会触发<font color=#00ffff>LogMessage</font>临时对象的析构，析构函数中则会调用通过其<font color=#00ffff>LogStream</font>成员指针调用`LogStream::Flush()`, 之后则交由服务当前正在使用的<font color=#00ffff>LogSink</font>对象(没有就使用<font color=#00ffff>DefaultLogSink</font>)来完成将<font color=#00ffff>LogStream</font>缓冲区的内容写入到日志文件

### ASyncLogSink  
由于<font color=#00ffff>DefaultLogSink</font>采用同步写日志文件的方式，并且每打印一条日志都会触发日志文件写入，效率较低，在日志量较大的场景下可能对系统性能影响比较大。因此mc-brpc提供了<font color=#00ffff>ASyncLogSink</font>对日志进行异步写入。其核心实现如下(详情请看源码)：
```c++
bool AsyncLogSink::OnLogMessage(
    int severity,
    const char* file,
    int line,
    const char* func,
    const butil::StringPiece& log_content) {
    if ((logging_dest & logging::LoggingDestination::LOG_TO_SYSTEM_DEBUG_LOG) != 0) {
        fwrite(log_content.data(), log_content.length(), 1, stderr);
        fflush(stderr);
    }

    if ((logging_dest & logging::LoggingDestination::LOG_TO_FILE) != 0) {
        LoggingLock logging_lock;
        _stream << log_content;
    }

    return true;
}

void AsyncLogSink::Run() {
    LOG(INFO) << "LogFlushThread start...";
    while (!is_asked_to_quit) {
        bthread_usleep(1000000);

        std::string s;
        {
            LoggingLock logging_lock;
            if (log_fd == -1) {
                Init();
            }

            s = _stream.str();
            _stream.str("");
        }

        if (log_fd != -1 && !s.empty()) {
            write(log_fd, s.c_str(), s.length());
        }
    }
}
```
其核心思想也比较简单，即在<font color=#00ffff>LogMessage</font>临时对象析构时，先将<font color=#00ffff>LogStream</font>缓冲区的数据加锁写入到一个全局的buffer中，再由后台线程每秒批量刷盘。经测试验证，通过这种方式写入日志性能相比<font color=#00ffff>DefaultLogSink</font>有10倍以上的提升，但是在程序崩溃的情况下，这种方式可能会导致最近1秒内日志丢失(可以通过先写到操作系统文件缓冲区但不刷新到磁盘进行优化改进)。想要使用<font color=#00ffff>ASyncLogSink</font>，只需要在CMakeLists.txt的add_server_source中增加<font color=#ffff00>ASYNCLOG</font>选项即可。

### 日志滚动压缩归档

为了方便日志查询，mc-brpc会默认按小时滚动切割并压缩日志文件，避免单个日志文件过大，同时方便日志查询；此外，mc-brpc会删除一段时间以前的日志(默认30天，可通过日志配置remain_days指定)，避免磁盘文件写满。滚动压缩通过<font color=#00ffff>LogArchiveWorker</font>和<font color=#00ffff>LogRotateWatcher</font>线程配合完成完成。其中，<font color=#00ffff>LogArchiveWorker</font>负责在整点通过`gzip`对上一小时的日志文件(.log)进行压缩，并按`%Y-%m-%d_%H`的时间格式生成对应压缩归档文件；<font color=#00ffff>LogRotateWatcher</font>则负责对当前正在写的日志文件(.log)进行事件监听，当当前监听文件发生删除或者移动事件时，生成一个新的.log日志文件并监听以及通知<font color=#00ffff>LogSink</font>关闭旧的文件句柄并重新打开新的日志文件进行写入。效果示例如下:
![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/47f788034c294ed2bface28cd10e1dde~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=2199&h=622&s=309680&e=png&b=2d2e28)

### 全链路日志
当代的互联网的服务，通常都是用复杂的、大规模分布式集群来实现的，不同服务间可能存在复杂的rpc调用关系，我们希望能通过某种方式将某个请求的调用链路日志全部串联起来便于问题排查，这属于分布式服务调用链路追踪的一部分，目前大部分分布式系统的跟踪系统实现都是基于[Google-Dapper](https://bigbully.github.io/Dapper-translation/)，brpc的[rpcz](https://github.com/apache/brpc/blob/master/docs/cn/rpcz.md)功能也实现了类似功能，但是没有支持将调用链路的日志串联起来，因此mc-brpc在brpc的基础上为日志前缀增加了`trace_id`，当我们把服务日志全部上报到ES等日志查询工具时，可以通过`trace_id`将整个请求链路的日志串联起来，便于问题分析排查。需要修改<font color=#ffffff>*PrintLogPrefix函数(butil/logging.cc)*</font>并在gflags开启`rpcz`功能:
```c++
// butil/logging.cc
void PrintLogPrefix(...) {
    // ...

    // add trace_id if has
    brpc::Span* span = (brpc::Span*)bthread::tls_bls.rpcz_parent_span;
    if(span) {
        os << " {trace_id:" << span->trace_id() << "}";
    }

    // ...
}
```
当开启`rpcz`功能后，brpc server在收到请求会创建一个`Span`对象(里面包含了上游传过来的trace_id，如果当前server是请求链路的第一个节点则会自动生成一个trace_id)，并将`Span`对象保存到bthread local对象(`bthread::tls_bls.rpcz_parent_span`)里面：  
```c++
// baidu_rpc_protocol.cpp
void ProcessRpcRequest(...) {
    // ...

    Span* span = NULL;
    if (IsTraceable(request_meta.has_trace_id())) {
        // 这里request_meta.trace_id()为上游传过来的trace_id，为0则自动生成一个trace_id
        span = Span::CreateServerSpan(
            request_meta.trace_id(), request_meta.span_id(),
            request_meta.parent_span_id(), msg->base_real_us());
        accessor.set_span(span);
        span->set_log_id(request_meta.log_id());
        span->set_remote_side(cntl->remote_side());
        span->set_protocol(PROTOCOL_BAIDU_STD);
        span->set_received_us(msg->received_us());
        span->set_start_parse_us(start_parse_us);
        span->set_request_size(msg->payload.size() + msg->meta.size() + 12);
    }

    // ...
    
    if (span) {
        span->set_start_callback_us(butil::cpuwide_time_us());
        span->AsParent();  // 等价于bthread::tls_bls.rpcz_parent_span = this;  将当前span对象设置为bthread local对象
    }

    /// ...
}
```
日志中打印的`trace_id`即从`bthread::tls_bls.rpcz_parent_span`中获取的。同时，当从当前服务向其它brpc服务发起rpc请求时，会将当前Span里面的trace_id写入到request_meta中传递下去，下游收到请求再从request_meta元数据中获取trace_id构建`Span`对象，因此也就可以通过`Span`对象里的trace_id将整个请求的日志串联起来：
```c++
// brpc/channel.cpp
void Channel::CallMethod(...) {
    // ...

    if (cntl->_sender == NULL && IsTraceable(Span::tls_parent())) {
        const int64_t start_send_us = butil::cpuwide_time_us();
        const std::string* method_name = NULL;
        if (_get_method_name) {
            method_name = &_get_method_name(method, cntl);
        } else if (method) {
            method_name = &method->full_name();
        } else {
            const static std::string NULL_METHOD_STR = "null-method";
            method_name = &NULL_METHOD_STR;
        }

        Span* span = Span::CreateClientSpan(
            *method_name, start_send_real_us - start_send_us);
        span->set_log_id(cntl->log_id());
        span->set_base_cid(correlation_id);
        span->set_protocol(_options.protocol);
        span->set_start_send_us(start_send_us);
        cntl->_span = span;   // here add span to cntl
    }

    // ...
}

// baidu_rpc_protocol.cpp
void PackRpcRequest(...) {
    // ...

    Span* span = accessor.span();  // here get span from cntl
    if (span) {
        request_meta->set_trace_id(span->trace_id());
        request_meta->set_span_id(span->span_id());
        request_meta->set_parent_span_id(span->parent_span_id());
    }

    SerializeRpcHeaderAndMeta(req_buf, meta, req_size + attached_size);
    req_buf->append(request_body);
    if (attached_size) {
        req_buf->append(cntl->request_attachment());
    }

    // ...
}
```

其中request_meta的定义如下(brpc/policy/baidu_rpc_meta.proto)：
```proto
message RpcRequestMeta {
    required string service_name = 1;
    required string method_name = 2;
    optional int64 log_id = 3;
    optional int64 trace_id = 4;
    optional int64 span_id = 5;
    optional int64 parent_span_id = 6;
    optional string request_id = 7;     // correspond to x-request-id in http header
    optional int32 timeout_ms = 8;      // client's timeout setting for current call
    optional string from_svr_name = 9;  // add by mc-brpc
}
```
其中，from_svr_name是mc-brpc在brpc的基础上新增的，用于被调用方获取上游服务名，如下：
```c++
brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
LOG(INFO) << "From:" << cntl->from_svr_name();
```

具体实现和trace_id的传递过程类似：
1. MCServer在启动时会将当前服务名保存到brpc::ServerOptions的server_info_name中：
```c++
MCServer::Start() {
    // ...
    brpc::ServerOptions options;
    options.server_info_name = utils::Singleton<ServerConfig>::get()->GetSelfName();
    if (_server.Start(point, &options) != 0) {
        LOG(ERROR) << "[!] Fail to start Server";
        exit(1);
    }
    // ...
}
```
2. 发起rpc请求时，将server_info_name写入到request_meta传递给下游：
```c++
// Channel::CallMethod(brpc/channel.cpp)
cntl->set_from_svr_name();

// PackRpcRequest(baidu_rpc_protocol.cpp) 
if(!cntl->from_svr_name().empty()) {
    request_meta->set_from_svr_name(cntl->from_svr_name());
}
```

3. 下游收到请求，从request_meta读取server_info_name并保存至当前请求Controller对象
```c++
// ProcessRpcRequest(baidu_rpc_protocol.cpp)
cntl->set_from_svr_name(request_meta.from_svr_name())
```

## DB连接管理
mc-brpc基于[libmysqlclient](https://dev.mysql.com/downloads/c-api/)提供了面向对象的MySQL的操作类<font color=#00ffff>MysqlConn</font>，并提供了对应的连接池实现<font color=#00ffff>DBPool</font>，通过<font color=#00ffff>DBPool</font>对MySQl连接进行管理(创建连接、释放连接、连接保活等)。
```c++
class DBPool {
private:
    std::string m_clusterName;  // 当前pool对应集群名称
    DbConfig m_dbConf;          // db参数配置（server.conf中指定)
    /**
        message DbConfig {
           string user = 1;
           string passwd = 2;
           string ip = 3;
           uint32 port = 4;
           string db_name = 5;
           uint32 max_active = 6;               // 最大活跃连接数
           uint32 min_idle = 8;                 // 最小空闲连接数
           uint32 ilde_timeout_ms = 9;          // 空闲连接超时时间(超过会自动释放连接)
           uint32 timeout_ms = 10;              // 获取连接超时时间
           uint32 refresh_interval_ms = 11;     // 连接刷新时间(空闲超过此时间会自动发送心跳至MySQL Server进行保活)
       }*/
    uint32_t m_curActive;  // 当前活跃连接数量

    std::queue<MysqlConn*> m_connQue;  // 空闲连接队列
    bthread::Mutex m_mtx;
    bthread::ConditionVariable m_cond;

    void CheckDbConfigs();
    bool AddConnect();
    void RecycleConnection();

public:
    DBPool(const std::string& cluster_name, const DbConfig& db_conf);
    DBPool(const DBPool& pool) = delete;
    DBPool& operator=(const DBPool& pool) = delete;
    DBPool(DBPool&& pool) = delete;
    DBPool& operator=(DBPool&& pool) = delete;
    ~DBPool();

    std::shared_ptr<MysqlConn> GetConnection();
};
```
<font color=#00ffff>DBPool</font>目前支持设置连接池最大活跃连接数、最小空闲连接数、空闲连接超时时间、获取连接超时时间、空闲连接保活刷新时间等。<font color=#00ffff>DBPool</font>通过一个队列保存当前空闲连接，获取时从队头取一个空闲链接，并返回连接对象<font color=#00ffff>MysqlConn</font>的shared_ptr指针，通过自定义shared_ptr的析构函数来实现shared_ptr销毁的时候将对应的<font color=#00ffff>MysqlConn</font>返还至空闲连接队列(由此可保证队头的连接既是空闲时间最长的连接)。获取连接操作因为需要操作共享的连接队列，因此必须加锁执行，但需要注意得使用`bthread::Mutex`，不能使用c++标准库提供的`std::Mutex`(前者是协程锁，阻塞bthread但不阻塞pthread；后者是线程锁，会直接导致pthread阻塞，性能差很多，甚至可能导致死锁现象)
```c++
/**
 * 获取从队列头部获取一个连接（也即空闲最久的连接），释放连接时自动将连接插入队列尾部
 */
std::shared_ptr<MysqlConn> DBPool::GetConnection() {
    std::unique_lock<bthread::Mutex> lck(m_mtx);
    // 当空闲连接队列为空则判断：1、当前最大活跃连接数已达上限，则等待  2、当前最大活跃连接数未达上限，新建连接
    if (m_connQue.empty()) {
        if (m_curActive >= m_dbConf.max_active()) {
            LOG(INFO) << "WaitForConnection, size:" << m_connQue.size() << ", active_size:" << m_curActive;
            int64_t timeout_us = m_dbConf.timeout_ms() * 1000;
            int64_t start_us = butil::gettimeofday_us();
            while (m_connQue.empty()) {
                int status = m_cond.wait_for(lck, timeout_us);
                if (status == ETIMEDOUT) {
                    // 获取链接超时返回nullptr
                    LOG(ERROR) << "[!] get db connection timeout:" << status << " [" << m_clusterName << "."
                               << m_dbConf.db_name() << "], conn_timeout_ms:" << m_dbConf.timeout_ms();
                    return nullptr;
                }
                timeout_us -= (butil::gettimeofday_us() - start_us);
            }
        } else {
            bool is_succ = AddConnect();
            if (!is_succ) {
                return nullptr;
            }
        }
    }

    // 自定义shared_ptr析构方法, 重新将连接放回到连接池中, 而不是销毁
    std::shared_ptr<MysqlConn> connptr(m_connQue.front(), [this](MysqlConn* conn) {
        std::unique_lock<bthread::Mutex> lck(this->m_mtx);
        conn->refreshFreeTime();
        LOG(DEBUG) << "release connection, id:" << conn->id();
        this->m_connQue.push(conn);
        this->m_curActive--;
        this->m_cond.notify_one();
    });

    LOG(DEBUG) << "get connection, id:" << connptr->id();
    m_connQue.pop();
    m_curActive++;
    return connptr;
}
```
同时<font color=#00ffff>DBPool</font>会维护一个后台线程，定期对长期空闲的连接进行释放以及发送心跳至MySQL以保活(默认情况下MySQL连接如果超过8小时没有请求会被服务端断开连接)，当当前空闲连接数大于最小空闲连接数时，对空闲超时的连接进行释放；否则，对第一个空闲超过指定刷新时间(默认1小时)的连接进行刷新(内部通过ping向MySQL Server发送心跳信息，避免连接被断开)并从队头移到队尾(这里每次只刷新一个连接，避免后台线程占锁太久)。
```c++
DBPool::DBPool(const std::string& cluster_name, const DbConfig& db_conf) :
        m_clusterName(cluster_name), m_dbConf(db_conf), m_curActive(0) {
    // 检查配置参数合法性并未未配置的参数指定默认值
    CheckDbConfigs();

    // 创建子线程用于销毁空闲链接
    std::thread(&DBPool::RecycleConnection, this).detach();

    LOG(INFO) << "db pool inited[" << m_clusterName << "]";
}

/**
 * 子线程--每5s执行一次检测并释放空闲(超时)连接，以及对剩余连接进行保活检测
 */
void DBPool::RecycleConnection() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::unique_lock<bthread::Mutex> lck(m_mtx);
        if ((m_connQue.size() > m_dbConf.min_idle())) {
            // 释放空闲超时的连接
            while (m_connQue.size() > m_dbConf.min_idle()) {
                MysqlConn* recyConn = m_connQue.front();
                if (recyConn->getFreeTime() >= m_dbConf.ilde_timeout_ms()) {
                    LOG(INFO) << "recycle connection, id:" << recyConn->id() << "[" << m_clusterName << "."
                              << m_dbConf.db_name() << "]";
                    m_connQue.pop();
                    delete recyConn;
                } else {
                    break;
                }
            }
        } else {
            // 对长时间空闲的连接进行保活检测，必要时重连
            if (!m_connQue.empty()) {
                MysqlConn* conn = m_connQue.front();
                if (conn->getFreeTime() > m_dbConf.refresh_interval_ms()) {
                    if (!conn->refresh()) {
                        // 刷新失败剔除连接
                        m_connQue.pop();
                    } else {
                        // 刷新成功将连接移到尾部
                        LOG(INFO) << "connection refreshed, id:" << conn->id();
                        conn->refreshFreeTime();
                        m_connQue.pop();
                        m_connQue.push(conn);
                    }
                }
            }
        }
    }
}
```
一般情况下，用户不需要手动创建<font color=#00ffff>DBPool</font>以及<font color=#00ffff>MysqlConn</font>，而是应该通过<font color=#00ffff>DBManager</font>从<font color=#00ffff>DBPool</font>里面获取<font color=#00ffff>MysqlCon</font>连接并进行操作。<font color=#00ffff>DBManager</font>是个单实例类`using DBManager = server::utils::Singleton<server::db::DbManager>;`  通过mc-brpc构建brpc服务时，如果需要用到MySQL，只需要：
1. 在CMakeLists.txt里面add_server_source添加<font color=#ffff00>USE_MYSQL</font>选项，这样会自动定义<font color=#ffff00>USE_MYSQL</font>宏并引入MySQL相关源文件和依赖库。
2. 在server.conf中配置好DB的相关配置，<font color=#00ffff>MCServer</font>启动会自动解析配置并为配置的每个DB集群创建对应的连接池对象(`DBManager::get()->Init();`)并注册到一个全局map中。
```c++
void DbManager::Init() {
    const google::protobuf::Map<std::string, DbConfig>& db_confs =
        server::utils::Singleton<ServerConfig>::get()->GetDbConfig();
    for (auto it = db_confs.begin(); it != db_confs.end(); ++it) {
        brpc::Extension<DBPool>::instance()->RegisterOrDie(it->first, new DBPool(it->first, it->second));
    }
}
```
使用示例如下：
```c++
// 从db_master集群获取一个MysqlConn连接
auto conn = DBManager::get()->GetDBConnection("db_master");
if (conn == nullptr) {
    // todo: error handler
    return;
}

std::string sql = "SELECT * FROM tbl_user_info limit 10";
try {
    if (conn->query(sql)) {
        while (conn->next()) {
            std::ostringstream os;
            for (unsigned i = 0; i < conn->cols(); ++i) {
                os << conn->value(i) << " ";
            }
            LOG(INFO) << os.str();
        }
    } else {
        LOG(ERROR) << "[!] db query error, sql:" << sql << ", err_no:" << conn->errNo()
                       << ", err_msg:" << conn->errMsg();
    }
} catch(std::exception& e) {
    LOG(ERROR) << e.what();
}
```

## Redis连接管理
mc-brpc基于[redis++](https://github.com/sewenew/redis-plus-plus)提供了Redis的操作类<font color=#00ffff>RedisWrapper</font>，它支持STL风格操作。与MySQL客户端代码不同的是，这里不需要单独实现连接池，因为[redis++](https://github.com/sewenew/redis-plus-plus)已经提供了这个功能，它会自动为配置里指定的redis实例(集群)维护一个连接池，但redis++对集群模式Redis和其它模式redis(单实例Redis、哨兵模式主从Redis)的操作分别是通过`sw::redis::RedisCluster`和`sw::redis::Redis`来完成的，那么如果我们服务里面原来使用的是单实例Redis，代码里是通过`sw::redis::Redis`来进行操作的，后面如果想切换到Redis集群，那就需要修改相关Redis操作代码，因此我们提供<font color=#00ffff>RedisWrapper</font>来屏蔽这些差异，业务层统一使用<font color=#00ffff>RedisWrapper</font>来进行Redis操作就行，<font color=#00ffff>RedisWrapper</font>内自动判断当前是对单实例Redis进行操作还是对集群模式Redis进行操作，简化业务代码开发。实现原理也很简单，通过`std::variant`(需要c++ 17及以上，低版本可以使用union代替)来存放`sw::redis::RedisCluster`和`sw::redis::Redis`, 操作redis时先判断当前`std::variant`中存放的实际类型再调对应类型的API，部分实例如下：
```c++
class RedisWrapper {
private:
    std::variant<Redis *, RedisCluster *> p_instance;

public:
    RedisWrapper(Redis *redis) : p_instance(redis) {}
    RedisWrapper(RedisCluster *cluster): p_instance(cluster) {}

    /**
     * @param key Key.
     * @param val Value.
     * @param ttl Timeout on the key. If `ttl` is 0ms, do not set timeout.
     * @param type Options for set command:
     *             - UpdateType::EXIST: Set the key only if it already exists.
     *             - UpdateType::NOT_EXIST: Set the key only if it does not exist.
     *             - UpdateType::ALWAYS: Always set the key no matter whether it exists.
     * @return Whether the key has been set.
     * @retval true If the key has been set.
     * @retval false If the key was not set, because of the given option.
     */
    bool set(const StringView &key,
             const StringView &val,
             const std::chrono::milliseconds &ttl = std::chrono::milliseconds(0),
             UpdateType type = UpdateType::ALWAYS) {
        if (std::holds_alternative<Redis*>(p_instance)) {
            return std::get<Redis*>(p_instance)->set(key, val, ttl, type);
        } else if (std::holds_alternative<RedisCluster*>(p_instance)) {
            return std::get<RedisCluster*>(p_instance)->set(key, val, ttl, type);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    /**
     @brief Set multiple fields of the given hash.
    *
    * Example:
    * @code{.cpp}
    *       std::unordered_map<std::string, std::string> m = {{"f1", "v1"}, {"f2", "v2"}};
    *       redis.hset("hash", m.begin(), m.end());
    * @endcode
    * @param key Key where the hash is stored.
    * @param first Iterator to the first field to be set.
    * @param last Off-the-end iterator to the given range.
    * @return Number of fields that have been added, i.e. fields that not existed before.
    */
    template <typename Input>
    auto hset(const StringView &key, Input first, Input last) ->
        typename std::enable_if<!std::is_convertible<Input, StringView>::value, long long>::type {
        if (std::holds_alternative<Redis *>(p_instance)) {
            return std::get<Redis *>(p_instance)->hset(key, first, last);
        } else if (std::holds_alternative<RedisCluster *>(p_instance)) {
            return std::get<RedisCluster *>(p_instance)->hset(key, first, last);
        } else {
            throw std::runtime_error("Redis p_instance not found.");
        }
    }

    // ...
};
```
同样，mc-brpc提供了单实例类<font color=#00ffff>RedisManager</font>(`using RedisManager = server::utils::Singleton<server::redis::redisManager>;`)类来完成解析Redis配置、为每个redis实例(集群)创建<font color=#00ffff>RedisWrapper</font>实例并注册到全局配置map中。用户一般情况下也不需要自己手动创建<font color=#00ffff>RedisWrapper</font>，而是应该使用<font color=#00ffff>RedisManager</font>来获取对应Redis实例(集群)的操作对象。通过mc-brpc构建brpc服务时，如果需要用到Redis，只需要：

1. 在CMakeLists.txt里面add_server_source添加USE_Redis选项，这样会自动定义USE_Redis宏并引入redis++相关源文件和依赖库。
2. 在server.conf中配置好Redis的相关配置，MCServer启动会自动解析配置并为配置的每个Redis实例(集群)创建对应的<font color=#00ffff>RedisWrapper</font>对象(`RedisManager::get()->Init();`)并注册到一个全局map中。
```c++
void redisManager::Init() {
    const google::protobuf::Map<std::string, RedisConfig>& redis_confs =
        server::utils::Singleton<ServerConfig>::get()->GetRedisConfig();
    for (auto it = redis_confs.begin(); it != redis_confs.end(); ++it) {
        if (it->second.has_redis_info()) {
            ConnectionOptions conn_options;
            conn_options.host = it->second.redis_info().host();
            conn_options.port = it->second.redis_info().port();
            conn_options.socket_timeout =
                std::chrono::milliseconds(it->second.timeout_ms() > 0 ? it->second.timeout_ms() : 1500);
            conn_options.password = it->second.passwd();

            ConnectionPoolOptions pool_options;
            pool_options.wait_timeout =
                std::chrono::milliseconds(it->second.wait_timeout_ms() > 0 ? it->second.wait_timeout_ms() : 1000);
            pool_options.size = it->second.pool_size() > 0 ? it->second.pool_size() : 3;

            // 注册Cluster/Redis信息
            if (it->second.redis_info().type() == "cluster") {
                RedisCluster* cluster = new RedisCluster(conn_options, pool_options);
                brpc::Extension<RedisWrapper>::instance()->RegisterOrDie(it->first, new RedisWrapper(cluster));
                LOG(INFO) << "redis cluster inited: " << it->first;
            } else {
                Redis* redis = new Redis(conn_options, pool_options);
                brpc::Extension<RedisWrapper>::instance()->RegisterOrDie(it->first, new RedisWrapper(redis));
                LOG(INFO) << "single redis inited: " << it->first;
            }
        } else if (it->second.has_sentine_info()) {
            SentinelOptions sentinel_opts;
            for (auto& sentine : it->second.sentine_info().sentines()) {
                sentinel_opts.nodes.emplace_back(make_pair(sentine.host(), sentine.port()));
            }
            sentinel_opts.socket_timeout =
                std::chrono::milliseconds(it->second.timeout_ms() > 0 ? it->second.timeout_ms() : 1500);
            auto sentinel = std::make_shared<Sentinel>(sentinel_opts);

            ConnectionOptions conn_options;
            conn_options.socket_timeout =
                std::chrono::milliseconds(it->second.timeout_ms() > 0 ? it->second.timeout_ms() : 1500);
            conn_options.password = it->second.passwd();

            ConnectionPoolOptions pool_options;
            pool_options.wait_timeout =
                std::chrono::milliseconds(it->second.wait_timeout_ms() > 0 ? it->second.wait_timeout_ms() : 1000);
            pool_options.size = it->second.pool_size() > 0 ? it->second.pool_size() : 3;

            // 注册Redis信息
            Redis* redis = new Redis(sentinel, "master_name", Role::MASTER, conn_options, pool_options);
            brpc::Extension<RedisWrapper>::instance()->RegisterOrDie(it->first, new RedisWrapper(redis));
            LOG(INFO) << "redis sentine inited: " << it->first;
        }
    }
}
```
使用示例如下：
```c++
// 从redis_cluster获取一个连接对象
auto cluster = RedisManager::get()->GetRedisConnection("redis_cluster");
if (!cluster) {
    return;
}
cluster->set("key", "value", std::chrono::milliseconds(3600000));

// 从单实例Redis获取一个连接对象
std::vector<std::string> ll = {"123", "abc", "PHQ"};
auto redis = RedisManager::get()->GetRedisConnection("redis_single");
if (!redis) {
    return;
}
redis->lpush("list_1", ll.begin(), ll.end());
redis->expire("list_1", std::chrono::seconds(3600));
```