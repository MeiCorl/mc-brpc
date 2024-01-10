# 简介
### &emsp;&emsp; mc-brpc是基于[百度brpc框架](https://brpc.apache.org/)快速开发rpc服务的集成框架，目的是简化构建brpc服务以及发起brpc请求的流程和代码开发量。主要在brpc的基础上增加了以下功能：
* **服务注册**：brpc对于rpc服务中一些功能并未提供具体实现，而是需要用户自己去扩展实现。如服务注册，因为不同用户使用的服务注册中心不一样，有Zookeeper、Eureka、Nacos、Consul、Etcd、甚至自研的注册中心等，不同注册中心注册和发现流程可能不一样，不太好统一，因此只好交由用户根据去实现服务注册这部分；而对于服务发现这部分，brpc默认支持基于文件、dns、bns、http、consul、nacos等的服务发现，并支持用户扩展NamingService实现自定义的服务发现。mc-brpc在brpc的基础上实现了服务自动注册到etcd，并提供了NamingService的扩展(<font color=#00ffff>McNamingService</font>)用以支持自定义的服务发现(**mc:\/\/service_name**)

* **名字服务代理**：mc-brpc服务启动会自动注册到etcd，但却不直接从etcd直接做服务发现，而是提供了一个<font color=#00ffff>brpc_name_agent</font>基础服务作为名字服务代理，它负责从etcd实时更新服务信息，并为mc-brpc提供服务发现，主要是为了支持在服务跨机房甚至跨大区部署时，brpc请求能支持按指定大区和机房进行路由，此外name_agent还将服务信息dump出来作为promethus监控的targets，以及后续拟支持户端主动容灾功(暂未实现)等

* **日志异步刷盘**：brpc提供了日志刷盘抽象工具类LogSink，并提供了一个默认的实现DefaultLogSink，但是DefaultLogSink写日志是同步写，且每写一条日志都会写磁盘，性能较差，在日志量大以及对性能要求较高的场景下很难使用，而百度内部使用的ComlogSink实现似乎未开源(看代码没找到)，因此mc-brpc自己实现了个<font color=#00ffff>AsyncLogSink</font>先将日志写缓冲区再批量刷盘(也得感谢brpc插件式的设计，极大的方便了用户自己做功能扩展)，AsyncLogSink写日志先写缓冲区，再由后台线程每秒批量刷盘，性能相比DefaultLogSink由10倍以上提升，但是在服务崩溃的情况下可能会丢失最近1s内的日志

* **日志自动归档**：<font color=#00ffff>LogRotateWatcher</font>及<font color=#00ffff>LogArchiveWorker</font>每小时对日志进行归档压缩，方便日志查询，并删除一个月以前的日志(时间可以通过配置指定)避免磁盘写满

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

    // 注册服务
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
4. 根据3中服务日志配置，初始化日志输出文件路径并创建日志文件监听线程和日志归档线程(二者配合实现日志分时归档压缩)，以及判断是否需要使用异步日志写入AsyncLogSink(需要CMakeLists.txt中定义<font color=#ffff00>USE_ASYNC_LOGSINK</font>宏)。
5. 根据3中服务配置，初始化DB连接池(需要CMakeLists.txt中定义<font color=#ffff00>USE_MYSQL</font>宏)及Redis连接池(需要CMakeLists.txt中定义<font color=#ffff00>USE_REDIS</font>宏)。
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
void MCServer::RegisterService() {
    ServerConfig* config = utils::Singleton<ServerConfig>::get();
    etcd::Client etcd(config->GetNsUrl());
    server::config::InstanceInfo instance;
    instance.set_region_id(config->GetSelfRegionId());
    instance.set_group_id(config->GetSelfGroupId());
    instance.set_endpoint(butil::endpoint2str(_server.listen_address()).c_str());
    etcd::Response resp = etcd.leasegrant(REGISTER_TTL).get();
    if (resp.error_code() != 0) {
        LOG(ERROR) << "[!] etcd failed, err_code: " << resp.error_code() << ", err_msg:" << resp.error_message();
        exit(1);
    }
    _etcd_lease_id = resp.value().lease();
    etcd::Response response =
        etcd.set(BuildServiceName(config->GetSelfName(), instance), instance.SerializeAsString(), _etcd_lease_id).get();
    if (response.error_code() != 0) {
        LOG(ERROR) << "[!] Fail to register service, err_code: " << response.error_code()
                   << ", err_msg:" << response.error_message();
        exit(1);
    }

    std::function<void(std::exception_ptr)> handler = [](std::exception_ptr eptr) {
        try {
            if (eptr) {
                std::rethrow_exception(eptr);
            }
        } catch (const std::runtime_error& e) {
            LOG(FATAL) << "[!] Etcd keepalive failure: " << e.what();
        } catch (const std::out_of_range& e) {
            LOG(FATAL) << "[!] Etcd lease expire: " << e.what();
        }
    };
    _keep_live_ptr.reset(new etcd::KeepAlive(config->GetNsUrl(), handler, REGISTER_TTL, _etcd_lease_id));
    LOG(INFO) << "Service register succ. instance: {" << instance.ShortDebugString()
              << "}, lease_id:" << _etcd_lease_id;
}

std::string MCServer::BuildServiceName(
    const std::string& original_service_name,
    const server::config::InstanceInfo& instance) {
    std::hash<std::string> hasher;
    return original_service_name + ":" + std::to_string(hasher(instance.SerializeAsString()));
}
```

### brpc_name_agent
mc-brpc服务并不直接从服务中心做服务发现，而是引入了一个基础服务brpc_name_agent作为名字服务代理。brpc_name_agent本身也是基于mc-brpc创建，它从etcd订阅了key事件通知，当有新的服务实例注册或者取消注册时，brpc_name_agent将对应服务实例信息添加至本地内存或者从本地内存移除(典型读多写少场景，使用DoublyBufferedData存储数据)。因此，每个name_agent实例都包含全部注册到etcd的服务实例信息。默认情况下，brpc_name_agent使用unix域套接字进行通信(unix:/var/brpc_name_agent.sock)，因此它不需要注册到etcd中，name_agent进程需要部署到每个服务器上供该服务器上的其他mc-brpc服务进程做服务发现。
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
假设上述文件路径为：/etc/prometheus/instance.json，则在为prometheus配置以下抓取任务即可实现对每个mc-brpc服务实例metrics监控：
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
        SingletonChannel::get()->GetChannel(_service_name, _group_strategy, _lb, &_options);`获取某个下游服务的channel并发起访问，其中`using SingletonChannel = server::utils::Singleton<ChannelManager>;`  但更多情况下我们不想使用这种，因为这样发起brpc请求的业务代码比较繁琐，首先要获取对应服务的channel，然后要用该channel初始化一个stub对象，同时还行要声明一个<font color=#00ffff>brpc::Controller</font>对象用于存储rpc请求元数据，最后在通过该stub对象发起rpc调用。这个过程对于每个brpc请求都是一样的，我们可以提供一个统一的实现，不用在业务层去写过多代码，也就是我们后续要提到的自动生成客户端代码部分，为需要调用的服务生成一个<font color=#00ffff>Client</font>对象，直接通过<font color=#00ffff>Client</font>对象就可以发起rpc调用。  

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

通过上述同步调用示例，可以看出发起rpc调用的流程较长，如果需要向多个不同服务的不同接口发起rpc调用需要编写大量重复类似的代码。因此，mc-brpc对此过程进行了简化，我们提供了个插件codexx（代码实现：core/extensions/codexx.cpp），它通过对目标服务的proto文件进行解析，为每个目标服务生成对应的同步客户端(<font color=#00ffff>SyncClient</font>)、半同步客户端(<font color=#00ffff>SemiSyncClient</font>)及异步客户端(<font color=#00ffff>ASyncClient</font>)，简化发起brpc调用的流程。只需要在CMakeList.txt中对添加下面两行即可：
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
服务编译后，会在client目录下生成对应的client源文件xxx.client.h和xxx.client.cpp(参考services/brpc_test/client/test.client.h), 之后引入对应client头文件便可以通过Client对象直接发起rpc调用，如下：  
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
client.Test(&req, &res, [](bool isFailed, TestRes* res) {
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
对比可发现，不同类型rpc调用客户端代码都简洁很多。示例里面只给出了最简单的用法，自动生成的Client类可以像brpc::Controller一样设置rpc连接超时时间、rpc超时时间、重试次数、LogId、获取rpc延迟时间等，以及设置rpc请求机房路由策略和负载均衡策略等。以下给出services/brpc_test/proto/test.proto生成的<font color=#00ffff>SyncClient</font>类的声明(其它请参考brpc_test/client/test.client.h和brpc_test/client/test.client.cpp)：
```c++
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
```