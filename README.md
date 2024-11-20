- [概述](#概述)
- [项目结构](#项目结构)
- [功能介绍](#功能介绍)
  - [1. MCServer](#1-mcserver)
    - [实例化过程](#实例化过程)
    - [启动过程](#启动过程)
    - [退出过程](#退出过程)
  - [2. 服务注册及发现](#2-服务注册及发现)
    - [服务注册](#服务注册)
    - [NameAgent名字服务代理](#nameagent名字服务代理)
    - [服务发现](#服务发现)
  - [3. 自动生成rpc客户端代码](#3-自动生成rpc客户端代码)
    - [原生brpc客户端调用流程示例](#原生brpc客户端调用流程示例)
    - [代码生成插件codexx引入](#代码生成插件codexx引入)
    - [ChannelManager](#channelmanager)
    - [同步调用](#同步调用)
    - [异步调用](#异步调用)
    - [半同步调用](#半同步调用)
  - [4. 高性能滚动异步日志](#4-高性能滚动异步日志)
    - [brpc流式日志写入流程](#brpc流式日志写入流程)
    - [ASyncLogSink](#asynclogsink)
    - [FastLogSink](#fastlogsink)
    - [LogSink性能对比](#logsink性能对比)
    - [日志滚动压缩归档](#日志滚动压缩归档)
    - [全链路日志](#全链路日志)
  - [5. bvar扩展支持多维label及prometheus采集](#5-bvar扩展支持多维label及prometheus采集)
    - [自定义bvar导出服务——MetricsService](#自定义bvar导出服务metricsservice)
    - [多维计数统计类——MetricsCountRecorder](#多维计数统计类metricscountrecorder)
    - [多维延迟统计类——MetricsLatencyRecorder](#多维延迟统计类metricslatencyrecorder)
    - [Server侧请求数统计](#server侧请求数统计)
    - [Client请求数统计](#client请求数统计)
    - [Server侧响应延迟统计](#server侧响应延迟统计)
    - [Client请求延迟统计](#client请求延迟统计)
    - [整体监控示例效果图](#整体监控示例效果图)
  - [6. 客户端主动容灾](#6-客户端主动容灾)
    - [RPC调用结果上报——LbStatClient](#rpc调用结果上报lbstatclient)
    - [RPC调用结果收集——LbStatSvr](#rpc调用结果收集lbstatsvr)
    - [熔断降级策略生成](#熔断降级策略生成)
      - [策略共享内存管理——StrategyShm](#策略共享内存管理strategyshm)
      - [策略生成类——StrategyGenerator](#策略生成类strategygenerator)
    - [策略应用](#策略应用)
  - [7. DB连接管理](#7-db连接管理)
    - [DBPool](#dbpool)
    - [DBManager](#dbmanager)
  - [8. Redis连接管理](#8-redis连接管理)
    - [RedisWrapper](#rediswrapper)
    - [RedisManager](#redismanager)
  - [9. 更多功能](#9-更多功能)

# 概述
 <font size=4>&emsp;&emsp; [**mc-brpc**](https://github.com/MeiCorl/mc-brpc)是基于[**百度brpc框架**](https://brpc.apache.org/)快速开发brpc服务的脚手架框架，目的是简化构建brpc服务的构建流程以及发起brpc请求的流程，减少业务代码开发量以及对原生brpc中一些功能进行优化及拓展。brpc是一个高性能、可扩展的RPC框架，广泛应用于许多大型互联网公司，mc-brpc大部分功能都通过在brpc基础上额外增加代码实现，一方面是出于代码隔离考虑，减少对原生brpc代码的入侵，另一方面则是利用原生brpc本身提供的很好的扩展性支持；仅少部分功能(全链路日志、多维bvar拓展、服务端响应QPS及延迟统计、客户端rpc请求QPS及延迟统计、客户端请求结果上报等)涉及对brpc源码修改。因此，mc-brpc在继承了brpc高性能、易扩展等优点的基础上增强及扩展了以下功能以更好的支持rpc服务开发：</font>
* **服务注册**：brpc对于rpc服务中一些功能并未提供具体实现，而是需要用户自己去扩展实现。如服务注册，brpc认为不同用户使用的服务注册中心不一样，有Zookeeper、Eureka、Nacos、Consul、Etcd、甚至自研的注册中心等，不同注册中心注册和发现机制可能不一样，不太好统一，因此只好交由用户根据实际需求去实现服务注册这部分；mc-brpc则在brpc的基础上增加了服务注册器<font color=#00ffff>ServiceRegister</font>抽象类，并提供了默认的<font color=#00ffff>EtcdServiceRegister</font>实现将服务自动注册到etcd(如果使用其它服务中心，可继承实现<font color=#00ffff>ServiceRegister</font>，并在server启动之前通过`MCServer::SetServiceRegister`方法替换服务注册器即可)
* **服务发现**: brpc通过<font color=#00ffff>NamingService</font>去做服务发现，默认支持了基于文件、dns、bns、http、consul、nacos等注册中心的服务发现，并支持用户扩展<font color=#00ffff>NamingService</font>实现自定义的服务发现。由于mc-brpc默认使用etcd做注册中心而brpc未提供基于etcd做服务发现的<font color=#00ffff>NamingService</font>实现，因此mc-brpc提供了<font color=#00ffff>NamingService</font>的扩展(<font color=#00ffff>McNamingService</font>)用以支持自定义协议(**mc:\/\/service_name**)的服务发现。目前，<font color=#00ffff>McNamingService</font>和brpc默认提供的<font color=#00ffff>NameService</font>一样都继承于<font color=#00ffff>brpc::PeriodicNamingService</font>，定时(默认每5秒)从服务中心更新服务实例信息，这种更新方式会存在一定的延时，未能很好利用etcd的事件通知功能，后续会考虑对此进行优化，借助etcd事件通知更实时更新服务实例信息
* **NameAgent名字服务代理**：mc-brpc服务启动会自动注册到etcd，但却不直接从etcd直接做服务发现，而是提供了一个<font color=#00ffff>brpc_name_agent</font>基础服务作为名字服务代理，它负责从etcd实时更新服务信息(基于etcd事件监听)，并为mc-brpc提供服务发现。这么做的目的主要有以下：1、name_agent将服务注册信息缓存到本机，并通过unix_socket为本机上的mc-brpc服务提供服务发现，效率更高；2、在服务跨机房甚至跨大区部署时服务时，希望能按指定大区策略和机房策略进行rpc请求路由，尽可能避免跨机房甚至跨大区rpc访问；3、mc-brpc支持将rpc调用结果上报至本机NameAgent(上报逻辑已预埋至brpc)，用于生成熔断降级策略，实现客户端主动容灾(暂未实现)；4、将服务信息dump出来作为promethus监控的targets
* **客户端主动容灾**：brpc框架默认支持的负载均衡路由算法基本都基于心跳检测，不具备主动容灾能力，具有延迟和误报的可能。mc-brpc会将每次rpc调用结果上报给<font color=#00ffff>LbStatClient</font>, 再通过一个独立上报线程周期性(默认200ms)的将结果通过unix socket发送给本机NameAgent进程进行统计并生成熔断降级策略后写入共享内存，为本机的其它业务进程提供主动容灾、熔断降级等能力
* **高性能异步日志**：brpc提供了日志刷盘抽象工具类<font color=#00ffff>LogSink</font>，并提供了一个默认的实现DefaultLogSink，但是DefaultLogSink写日志是同步写，且每写一条日志都会写磁盘，性能较差，在日志量大以及对性能要求较高的场景下很难使用，而百度内部使用的ComlogSink实现似乎未开源(看代码没找到)，因此mc-brpc提供了<font color=#00ffff>AsyncLogSink</font>和<font color=#00ffff>FastLogSink</font>用于高性能写日志；`AsyncLogSink`采用异步批量刷盘的方式，先将日志写到缓冲区，再由后台线程每秒批量刷盘(服务崩溃的情况下可能会丢失最近1s内的日志)；`FastLogSink`则基于`mmap`文件内存映射将写文件操作转为内存操作，减少系统调用次数以实现高性能写入；经测试验证，二者性能相比`DefaultLogSink`都有10倍以上提升

* **日志自动滚动归档**：<font color=#00ffff>LogRotateWatcher</font>及<font color=#00ffff>LogArchiveWorker</font>每小时对日志进行滚动压缩归档，方便日志查询，并删除一段时间(默认1个月，可以公共通过log配置的remain_days属性指定)以前的日志，避免磁盘写满

* **自动生成rpc客户端代码**：当某个服务通过proto文件定义好接口后，其它服务若想调用该服务的接口，只需要在CMakeLists.txt(参考services/brpc_test/CmakeLists.txt)中调用<font color=#ffff00>auto_gen_client_code</font>即可为指定proto生成对应的同步客户端(<font color=#00ffff>SyncClient</font>)、半同步客户端(<font color=#00ffff>SemiSyncClient</font>)及异步客户端(<font color=#00ffff>ASyncClient</font>)，简化发起brpc调用的流程。其原理是通过一个mc-brpc提供的protobuf插件<font color=#00ff00>codexx</font>(core/plugins/codexx)解析对应proto文件然后生成相应客户端代码

* **扩展bvar支持多维度统计，无缝接入prometheus，自动统计作为服务端响应及作为客户端请求的QPS及延迟**：原生brpc的bvar导出时不支持带label信息([here](https://github.com/apache/brpc/issues/765))，虽然提供了[mbvar](https://github.com/apache/brpc/blob/master/docs/cn/mbvar_c%2B%2B.md#bvarmvariables)来支持多维度统计，但其导出格式不支持Prometheus采集；因此mc-brpc实现了<font color=#00ffff>MetricsCountRecorder</font>和<font color=#00ffff>MetricsLatencyRecorder</font>来分别实现多维计数统计及多维延迟统计，支持自定义mtrices label并按Prometheus采集数据格式导出，更便于多维数据监控上报及统计；目前，mc-brpc基于<font color=#00ffff>MetricsCountRecorder</font>和<font color=#00ffff>MetricsLatencyRecorder</font>实现了自动统计当前服务作为服务端响应及作为客户端请求的QPS和延迟。

* **DB连接管理**：core/mysql下基于[libmysqlclient](https://dev.mysql.com/downloads/c-api/)封装实现了<font color=#00ffff>MysqlConn</font>并提供了对应连接池实现<font color=#00ffff>DBPool</font>，mc-brpc服务启动时会根据配置按需为每个DB实例初始化一个连接池对象并注册到一个全局map中，无须用户编写任何代码，使用时直接通过<font color=#00ffff>DBManager</font>从对应实例的连接池获取一个连接进行DB操作。DBPool支持设置连接池最小空闲连接数、最大活跃连接数、获取链接超时时间、连接空闲超时时间（长期空闲超时的连接会被自动释放）等

* **Redis连接管理**：core/redis下基于[redis-plus-plus](https://github.com/sewenew/redis-plus-plus)封装实现了客户端代理类<font color=#00ffff>RedisWrapper</font>(主要目的在于对业务代码屏蔽redis++在操作单实例redis、哨兵模式redis以及集群模式redis的差异)。同DB连接管理一样，mc-brpc服务启动时会根据配置为每个Redis(单实例/Sentine/Cluster)初始化一个RedisWrapper对象并注册到一个全局map中，使用时通过<font color=#00ffff>RedisManager</font>从对应redis(集群)获取一个连接进行操作。这里redis连接池的管理就不需要做额外实现了，redis++会为每个redis实例创建一个连接池并管理连接(如果是集群则为每个master节点创建一个连接池)

# 项目结构
├── brpc   &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp; # brpc源码(做了少量修改)  
├── core   &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp; # 新增的拓展及其他功能都在这个目录下     
│   ├── common         &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp; # 一些常用公共代码实现   
│   ├── config         &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp; # 服务配置类代码    
│   ├── extensions     &emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp; # 对brpc中一些功能的扩展  
│   ├── lb_stat        &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp; # rpc调用结果上报、熔断降级策略相关代码   
│   ├── log            &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;&nbsp; # 日志组件扩展  
│   ├── mysql          &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp; # MySQL连接池及工具类实现  
│   ├── plugins        &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp; # 插件工具，目前仅包含codexx的实现，用户根据proto文件自动生成客户端代码  
│   ├── redis          &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp; # Redis客户端代理及工具类实现  
│   ├── server         &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp; # 对brpc server的一些封装   
│   └── utils          &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp; # 一些工具类实现(感觉可以放到common目录)   
└── services           &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp; # 基于mc-brpc开发的brpc服务示例  
│       ├── brpc_name_agent    &emsp;&emsp;&nbsp; # 名字服务代理、rpc调用结果收集、客户端熔断降级策略生成(作为一个基础服务，需要打包部署到每个容器镜像中，为该容器内的其它服务提供服务发现)  
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

    // 创建brpc server（会额外触发server.conf全局配置解析)
    _server = new brpc::Server(utils::Singleton<ServerConfig>::get()->GetSelfName());

    // 初始化日志
    LoggingInit(argv);

    // 注册名字服务(McNamingService)
    RegisterNamingService();

    // 初始化服务注册器(默认使用EtcdServiceRegister)
    _service_register.reset(new brpc::policy::EtcdServiceRegister);

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
1. 判断是否有gflags.conf配置文件(../conf/gflags.conf)，如果有则自动触发gflags解析。可以在这里通过gflags配置控制一些brpc功能的开关(如优雅退出、rpcz等，参考services/brpc_test/conf/gflags.conf)，以及设置自定义gflags参数。
2. 解析服务配置文件(../conf/server.conf)，里面包含注册中心地址、服务名、所在大区id、机房id、DB配置、Redis配置、日志配置等(参考services/brpc_test/conf/server.conf)。
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
3. <font color=#00ffff>MCServer</font>包含了brpc::Server的指针，因此需要创建brpc::Server的实例。
4. 根据2中服务日志配置，初始化日志输出文件路径并创建日志文件监听线程和日志归档线程(二者配合实现日志分时压缩归档)，以及判断是否需要使用异步日志AsyncLogSink或者FastLogSink(CMakeLists.txt中add_server_source添加<font color=#ffff00>ASYNCLOG</font>选项或者<font color=#ffff00>FASTLOG</font>, 如`add_server_source(SERVER_SRCS ASYNCLOG)`, 这样会触发<font color=#ffff00>USE_ASYNC_LOGSINK</font>宏定义)。
5. 注册自定义的名字服务<font color=#00ffff>McNamingService</font>用以支持**mc:\/\/** 协议的服务发现
6. 初始化服务注册器，默认使用<font color=#00ffff>EtcdServiceRegister</font>将服务信息注册到etcd，如果使用其它类型的注册中心，需要继承<font color=#00ffff>ServiceRegister</font>实现自己的服务注册器，并在MCServer启动前通过`MCServer::SetServiceRegister`替换当前服务注册器。
7. 根据2中服务配置，初始化DB连接池(CMakeLists.txt中add_server_source添加<font color=#ffff00>USE_MYSQL</font>选项，触发定义<font color=#ffff00>USE_MYSQL</font>宏定义以及MySQL相关源文件及依赖库引入)及Redis连接池(CMakeLists.txt中add_server_source添加<font color=#ffff00>USE_REDIS</font>选项，触发<font color=#ffff00>USE_REDIS</font>宏定义及相关源文件和依赖库引入)。

### 启动过程
1. 首先指定服务监听地址。可以通过gflags<*listen_addr*>指定ip:port或者unix_socket地址, 否则使用当前ip地址+随机端口。调brpc::Start启动server
2. 将服务信息(ip:port、服务名、大区id、机房id)注册到服务中心。这里使用etcd作为服务中心，并通过etcd的租约续期功能为注册信息续期。
3. 初始化rpc调用结果上报客户端类实例
4. 调brpc::RunUntilAskedToQuit死循环直到进程被要求退出。  
```c++
void MCServer::Start(bool register_service) {
    // start brpc server
    butil::EndPoint point;
    if (!FLAGS_listen_addr.empty()) {
        butil::str2endpoint(FLAGS_listen_addr.c_str(), &point);
    } else {
#ifdef LOCAL_TEST
        butil::str2endpoint("", 0, &point);
#else
        char ip[32] = {0};
        NetUtil::getLocalIP(ip);
        butil::str2endpoint(ip, 0, &point);
#endif
    }
    brpc::ServerOptions options;
    options.server_info_name = ServerConfig::GetInstance()->GetSelfName();
    if (_server->Start(point, &options) != 0) {
        LOG(ERROR) << "[!] Fail to start Server";
        exit(1);
    }

    // register service if necessary
    if (register_service) {
        if (!_service_register) {
            LOG(ERROR) << "[!] service register not found!";
            exit(1);
        }

        if (!_service_register->RegisterService()) {
            exit(1);
        }
    }

    // init lb stat
    server::lb_stat::LbStatClient::GetInstance()->Init();

    // loop
    _server->RunUntilAskedToQuit();
}
```

### 退出过程
退出过程相对简单。销毁日志监听线程、日志归档线程、rpc上报线程等，从服务中心取消注册并停止etcd租约续期等。

## 2. 服务注册及发现
### 服务注册
MCSever启动会时，会从服务配置里获取注册中心地址、当前服务名、大区id、及机房id等，并通过<font color=#00ffff>ServiceRegister</font>的实现类<font color=#00ffff>EtcdServiceRegister</font>向etcd注册服务信息，这里使用到了[etcd-cpp-apiv3](https://github.com/etcd-cpp-apiv3/etcd-cpp-apiv3)。注册key为"*服务名:实例id*"(这里实例id设计比较简单，直接用实例信息序列化后的字符串计算hash code，暂时没有实际用处), value为实例信息(server::config::InstanceInfo, 参考core/config/server_config.proto)序列化后字符串。
```c++
bool EtcdServiceRegister::RegisterService() {
    ServerConfig* config = Singleton<ServerConfig>::get();
    etcd::Client etcd(config->GetNsUrl());
    server::config::InstanceInfo instance;
    instance.set_region_id(config->GetSelfRegionId());
    instance.set_group_id(config->GetSelfGroupId());
    instance.set_endpoint(butil::endpoint2str(brpc::Server::_current_server->listen_address()).c_str());
    etcd::Response resp = etcd.leasegrant(REGISTER_TTL).get();
    if (resp.error_code() != 0) {
        LOG(ERROR) << "[!] etcd failed, err_code: " << resp.error_code() << ", err_msg:" << resp.error_message();
        return false;
    }
    _etcd_lease_id = resp.value().lease();

    _register_key = BuildServiceName(config->GetSelfName(), instance);
    etcd::Response response = etcd.set(_register_key, instance.SerializeAsString(), _etcd_lease_id).get();
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
        auto fn = std::bind(&EtcdServiceRegister::TryRegisterAgain, this);
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
void EtcdServiceRegister::TryRegisterAgain() {
    do {
        LOG(INFO) << "Try register again.";
        bool is_succ = RegisterService();
        if (is_succ) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    } while (true);
}

void EtcdServiceRegister::UnRegisterService() {
    if (_keep_live_ptr) {
        _keep_live_ptr->Cancel();
    }

    ServerConfig* config = Singleton<ServerConfig>::get();
    etcd::Client etcd(config->GetNsUrl());
    etcd.rm(_register_key);
}
```

### NameAgent名字服务代理
mc-brpc服务并不直接从服务中心做服务发现，而是引入了一个基础服务brpc_name_agent作为名字服务代理。brpc_name_agent本身也是基于mc-brpc创建，它包含两个功能模块<font color=#00ffff>NameServiceProxy</font>和<font color=#00ffff>LbStatSvr</font>(LbStatSvr此处不做过多介绍，后续客户端主动容灾章节详细介绍)。其中<font color=#00ffff>NameServiceProxy</font>作用主要是最为名字服务的代理，它从etcd订阅了key事件通知，当有新的服务实例注册或者取消注册时，<font color=#00ffff>NameServiceProxy</font>将对应服务实例信息添加至本地内存或者从本地内存移除(典型读多写少场景，使用<font color=#00ffff>DoublyBufferedData</font>存储数据)。因此，每个name_agent实例都包含全部注册到etcd的服务实例信息。默认情况下，brpc_name_agent使用unix域套接字进行通信(unix:/var/brpc_name_agent.sock)，因此它不需要注册到etcd中，name_agent进程需要部署到每个服务器上供该服务器上的其他mc-brpc服务进程做服务发现。
```c++
void NameServiceProxy::WatcherCallback(etcd::Response response) {
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
2. 客户端主动容灾(暂未实现)。brpc的主要client容灾手段为心跳检查，即一个后端连接如果断掉且重连失败，会从名字里摘掉。心跳检查可以解决一部分问题，但是有其局限性。当后端某个ip cpu负载高，网络抖动，丢包率上升等情况下，一般心跳检查是能通过的，但是此时我们判断该ip是处于一种异常状态，需要降低访问权重，来进行调节。为此，需要收集并统计rpc调用，生成熔断降级策略，name_agent进程的<font color=#00ffff>LbStatSvr</font>模块负责完成这部分工作。
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
            "region_id":"2",
            "group_id":"2001"
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
  - job_name: "default_metrics"
    metrics_path: "brpc_metrics"
    file_sd_configs:
    - refresh_interval: 10s
      files:
        - /etc/prometheus/instance.json
```
这里我们配置了一个Prometheus指标抓取任务default_metrics，监控目标从/etc/prometheus/instance.json去发现，*注意: metrics_path需要指定为brpc_metrics, brpc服务默认metrics到处路径为/brpc_metrics(后面我们还会新增一个mc_server_metrics的任务，用于从/metrics抓取我们自定义的多维指标)*, 以下是通过[grafana](https://grafana.com/)对prometheus采集的服务指标进行可视化的示例：
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
### 原生brpc客户端调用流程示例
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

### 代码生成插件codexx引入
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
服务编译后，会在client目录下生成对应的client源文件xxx.client.h和xxx.client.cpp。下面给出部分services/brpc_test/proto/test.proto编译后自动生成的客户端代码(*注意：不同类型客户端代码实现会有些差异，具体见源码*):  
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
### ChannelManager
注意到上述生成代码中`SingletonChannel::get()`，它用于获取一个全局单例的<font color=#00ffff>ChannelManager</font>对象，再通过它的`ChannelManager::GetChannel`方法按指定的大区机房策略以及负载均衡策略创建(`Channel::Init`)面向目标服务的`brpc::Channel`指针，并缓存至本地。注意：`Channel::Init`不是线程安全的，需要加锁，而之后对Channel的使用是线程安全的，因此可以缓存并共享，避免反复创建；其次，缓存Channel时的key是*service_name + group_strategy + lb*，意味着下次通过相同大区机房策略及lb策略访问相同服务时才能共享使用之前已经创建的Channel
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
### 同步调用
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
### 异步调用
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

### 半同步调用
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


## 4. 高性能滚动异步日志
### brpc流式日志写入流程
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
1. **获取当前线程里的LogStream对象**：通过`LOG(security_level)`宏展开后会生成一个<font color=#00ffff>LogMessage</font>临时对象, 并通过该临时对象的`stream()`方法返回一个<font color=#00ffff>LogStream</font>类型的成员指针，该成员指针在LogMessage对象构造的时候被初始化为指向一个ThreadLocal的<font color=#00ffff>LogStream</font>对象，因此即相当于获取当了当前线程的<font color=#00ffff>LogStream</font>对象。
2. **将日志写入到LogStream对象**：<font color=#00ffff>LogStream</font>继承了std::ostream，因此可以通过c++流插入运算符(`<<`)直接向其中写入日志数据，这里写入的时候会先往<font color=#00ffff>LogStream</font>中写入一些前缀信息，如日志级别、打印时间、文件名及代码行数、函数名(mc-brpc新增了trace_id，便于服务调用全链路追踪)等，之后再是写入用户想要写入的日志内容
3. **日志持久化到文件**：<font color=#00ffff>LogStream</font>只充当了个缓冲区，还需要将缓冲区的内容写入到文件进行持久化，brpc将这部分功能交由<font color=#00ffff>LogSink</font>来完成(用户可以继承LogSink类并重写`LogSink::OnLogMessage`方法来实现定制化的写入)。具体实现为当LOG所在代码行结束时则会触发<font color=#00ffff>LogMessage</font>临时对象的析构，析构函数中则会调用通过其<font color=#00ffff>LogStream</font>成员指针调用`LogStream::Flush()`, 之后则交由服务当前正在使用的<font color=#00ffff>LogSink</font>对象(没有就使用<font color=#00ffff>DefaultLogSink</font>)来完成将<font color=#00ffff>LogStream</font>缓冲区的内容写入到日志文件

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
其核心思想也比较简单，即在<font color=#00ffff>LogMessage</font>临时对象析构时，先将<font color=#00ffff>LogStream</font>缓冲区的数据加锁写入到一个全局的buffer中，再由后台线程每秒批量刷盘。经测试验证，通过这种方式写入日志性能相比<font color=#00ffff>DefaultLogSink</font>有10倍以上的提升，但是在程序崩溃的情况下，这种方式可能会导致最近1秒内日志丢失(可以通过coredump文件查找丢失的日志)。想要使用<font color=#00ffff>ASyncLogSink</font>，只需要在CMakeLists.txt的add_server_source中增加<font color=#ffff00>ASYNCLOG</font>选项即可。

### FastLogSink
<font color=#00ffff>ASyncLogSink</font>性能虽好，但在程序崩溃的时候，如果前一秒的日志数据还未来得及刷盘，则会丢失，因此可能会导致程序崩溃原因的关键日志丢失(虽然可以通过coredump文件产看丢失的日志，但终归增加了问题排查成本)，因此mc-brpc提供了<font color=#00ffff>FastLogSink</font>，基于`mmap`文件内存映射来实现高性能日志写入，其性能除在多线程高并发写入时略低于<font color=#00ffff>ASyncLogSink</font>，其它大部分情况下二者性能接近。核心实现如下：
```c++
bool FastLogSink::OnLogMessage(
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
        if (Init()) {
            while (cur_addr + log_content.length() >= end_addr) {
                AdjustFileMap();
            }
            memcpy(cur_addr, log_content.data(), log_content.length());
            cur_addr += log_content.length();
        }
    }

    return true;
}

void FastLogSink::AdjustFileMap() {
    size_t file_size = 0;
    struct stat fileStat;
    fstat(log_fd, &fileStat);
    file_size = fileStat.st_size;

    size_t cur_pos = 0;  // 当前写入位置相对于文件头的offset
    if (begin_addr != nullptr && end_addr != nullptr) {
        cur_pos = file_size - (end_addr - cur_addr);
        munmap(begin_addr, FLAGS_map_trunk_size);
    } else {
        cur_pos = file_size;
    }

    // 下次映射从当前写入地址(cur_pos)的上一文件页末尾开始(会产生长度为file_size-offset的区域被重复映射)
    size_t offset = RoundDownToPageSize(cur_pos);
    size_t new_file_size = file_size + (FLAGS_map_trunk_size - (file_size - offset) + 1);
    ftruncate(log_fd, new_file_size);

    begin_addr = (char*)mmap(NULL, FLAGS_map_trunk_size, PROT_READ | PROT_WRITE, MAP_SHARED, log_fd, offset);
    if (begin_addr == MAP_FAILED) {
        std::ostringstream os;
        os << "mmap failed, errno:" << errno << ", log_fd:" << log_fd << std::endl;
        std::string&& err = os.str();
        fwrite(err.c_str(), err.size(), 1, stderr);
        exit(1);
    }
    cur_addr =
        begin_addr + (cur_pos - offset);  // 当前写入位置应为起始地址加上上次映射写入位置相对于当前映射起始地址的偏移量
    end_addr = begin_addr + FLAGS_map_trunk_size;
}
```
OnLogMessage写日志时，直接将日志内容拷贝至文件映射的共享内存中，避免io操作；期间如果共享内存剩余空间不足，则需要重新映射文件，增加映射内存区域大小，每次你新增映射区域大小有gflags参数map_trunk_size指定(默认4kb)，每次调整内存大小后，需要调整当前写入地址的offset以确保从上次写入的位置之后开始写入，避免将旧数据覆盖。同理，想要使用<font color=#00ffff>FastLogSink</font>，只需要在CMakeLists.txt的add_server_source中增加<font color=#ffff00>FASTLOG</font>选项即可

### LogSink性能对比
这里我在我本地机器(4核, Intel(R) Core(TM) i5-9500 CPU @ 3.00GHz  3.00 GHz，普通机械硬盘)Ubuntu docker容器上对<font color=#00ffff>ASyncLogSink</font>、<font color=#00ffff>FastLogLogSink</font>以及默认的<font color=#00ffff>DefaultLogSink</font>做了个简单的性能测试，测试内容为用三种<font color=#00ffff>LogSink</font>插件分别写入1kw条日志(每条日志100B)耗时, 分单线程写入1kw条数据以及10线程并发写入(每个线程写入1百万条)，测试结果(耗时为5次取平均值)如下：<center>
|                       |   __单线程耗时(ms)__    |   __多线程耗时(ms)__  |
|       :----:          |           :----:       |          :----:      |
| __DefaultLogSink__    |           33420        |        149019        |
| __ASyncLogSink__      |           10334        |         6976         |
| __FastLogLogSink__    |           10895        |        14600         |
</center>

可以看出，在单线程情况下<font color=#00ffff>ASyncLogSink</font>和<font color=#00ffff>FastLogLogSink</font>性能接近，是<font color=#00ffff>DefaultLogSink</font>的3倍；多线程并发写入情况下，<font color=#00ffff>ASyncLogSink</font>表现最优，<font color=#00ffff>FastLogLogSink</font>略差，但也是<font color=#00ffff>DefaultLogSink</font>的10倍性能。<font color=#00ffff>ASyncLogSink</font>和<font color=#00ffff>FastLogLogSink</font>均能实现每秒百万级日志写入，但<font color=#00ffff>ASyncLogSink</font>是写入到应用层缓冲区，进程崩溃会导致未刷盘的数据丢失，<font color=#00ffff>FastLogLogSink</font>是写入文件内存映射中，即便进程异常崩溃，会由操作系统将内存数据写入磁盘。

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
## 5. bvar扩展支持多维label及prometheus采集
### 自定义bvar导出服务——MetricsService
原生brpc的bvar导出时不支持带label信息([here](https://github.com/apache/brpc/issues/765))，虽然提供了[mbvar](https://github.com/apache/brpc/blob/master/docs/cn/mbvar_c%2B%2B.md#bvarmvariables)来支持多维度统计，但其导出格式不支持Prometheus采集；如当通过mbvar统计brpc_test服务每个接口(总共两个接口UpdateUserInfo、Test)请求次数时，原生brpc统计结果导出后如下：
```yaml
# HELP service_request_counter{method="Test"}
# TYPE service_request_counter{method="Test"} gauge
service_request_counter{method="Test"} 14920
# HELP service_request_counter{method="UpdateUserInfo"}
# TYPE service_request_counter{method="UpdateUserInfo"} gauge
service_request_counter{method="UpdateUserInfo"} 14920
```
这个输出格式是不支持被prometheus采集的，期望的输出格式应该如下:
```yaml
# HELP service_request_counter service_request_counter
# TYPE service_request_counter gauge
service_request_counter{method="Test"} 270873
service_request_counter{method="UpdateUserInfo"} 270843
```
因此mc-brpc实现了自己的多维指标统计方式，可支持自定义mtrices label并按上述Prometheus采集数据格式导出，更便于多维数据监控上报及统计。为此，mc-brpc提供了`bvar::MetricsCountRecorder`和`bvar::MetricsLatencyRecorder`分别用于多维的计数统计和多维的延迟统计，一个`MetricsCountRecorder`(`MetricsLatencyRecorder`)对象可以看做是一堆bvar的集合，这些bvar具有相同的名字(metrics_name)，相同的标签(label)，但是label取值不同(具体实现请看源码bvar/metrics_count_recorder.h 和 brpc/bvar/metrics_latency_recorder.h)。此外，mc-brpc在brpc基础上新增了一个内置服务<font color=#00ffff>MetricsService</font>，用于将自定义多维bvar在/metrics路径按prometheus采集数据格式导出。
```c++
// brpc/server.cpp
int Server::AddBuiltinServices() {
    // ...
    if (AddBuiltinService(new (std::nothrow) PrometheusMetricsService)) {
        LOG(ERROR) << "Fail to add PrometheusMetricsService";
        return -1;
    }

    // ****  here we add MetricsService  ****/
    if (AddBuiltinService(new (std::nothrow) MetricsService)) {
        LOG(ERROR) << "Fail to add MetricsService";
        return -1;
    }

    // ...
}
```
与brpc原有的<font color=#00ffff>PrometheusMetricsService</font>类不同，<font color=#00ffff>MetricsService</font>支持以下功能：

1. bvar::Dumper类新增dump_metrics接口，只会导出`MetricsCountRecorder`和`MetricsLatencyRecorder`中的bvar。
2. DisplayFilter原本只有3个选项将bvar分为plain txt类型，html类型与all类型（既是plain txt，也是html类型），在此基础上新增metrics类型选项DISPLAY_METRICS_DATA，这个过滤器的作用十分大，因为`bvar::MetricsCountRecorder`与`bvar::MetricsLatencyRecorder`类里面包含很多已经被expose的bvar了，不做处理的话这些bvar会被输出到/vars页面或者是/brpc_metrics或者被dump出到文件的，为了防止这些bvar被输出，需要设置一个新的DisplayFilter选项，并且只有/metrics能导出这些类型的bvar。
3. 导出到`MetricsService`的带Prometheus metrics信息的bvar会依照metrics name聚合，相同metrics name的bvar数据会按照Prometheus metrics的数据格式输出。

### 多维计数统计类——MetricsCountRecorder
`MetricsCountRecorder`用于多维计数统计(可用于统计QPS等数据), 支持添加固定label和自定义label。使用方法如下：
```c++
bvar::MetricsCountRecorder<uint64_t> counter("request_counter", "request for each method"); // 定义一个统计接口请求次数的计数器
counter.set_metrics_label("service_name", "brpc_test");   // 设置一个固定标签service_name，其值固定为brpc_test
counter.set_metrics_label("method");    // 设置一个(非固定标签)method
counter.find({"Test1"}) << 5;   // Test1接口被请求了5次
counter.find({"Test1"}) << 1;   // Test1接口又被请求了1次
counter.find({"Test2"}) << 12;  // Test2接口被请求了12次
```
访问/metrics路径，将可以得到如下prometheus格式输出：
```yaml
# HELP request_counter request for each method
# TYPE request_counter gauge
request_counter{service_name="brpc_test", method="Test1"} 6
request_counter{service_name="brpc_test", method="Test2"} 12
```
为方便使用我们在core/common/metrcis_helper.h提供了些常用的宏定义：
```c++
#pragma once
#include "bvar/metrics_count_recorder.h"

///////// define
#define DEFINE_METRICS_counter_u64(name, metrics_name, desc)                         \
    namespace mc {                                                                   \
    namespace mc_metrics {                                                           \
    bvar::MetricsCountRecorder<uint64_t> METRICS_COUNTER_##name(metrics_name, desc); \
    }                                                                                \
    }                                                                                \
    using mc::mc_metrics::METRICS_COUNTER_##name

///////// declare
#define DECLARE_METRICS_counter_u64(name)                               \
    namespace mc {                                                      \
    namespace mc_metrics {                                              \
    extern bvar::MetricsCountRecorder<uint64_t> METRICS_COUNTER_##name; \
    }                                                                   \
    }                                                                   \
    using mc::mc_metrics::METRICS_COUNTER_##name

///////// set fixed labels
#define ADD_STATIC_COUNTER_LABEL(name, label_key, label_val) \
    mc::mc_metrics::METRICS_COUNTER_##name.set_metrics_label(label_key, label_val)

//////// set unfixed labels
#define ADD_COUNTER_LABEL(name, label_key) mc::mc_metrics::METRICS_COUNTER_##name.set_metrics_label(label_key)

//////// set metrics
#define ADD_METRICS_COUNTER(name, args...)          mc::mc_metrics::METRICS_COUNTER_##name.find({args}) << 1
#define ADD_METRICS_COUNTER_VAL(name, val, args...) mc::mc_metrics::METRICS_COUNTER_##name.find({args}) << val
```

以统计brpc_test服务下每个接口qps为例，使用方式如下：
```c++
// 定义metrics(service_request_counter)
DEFINE_METRICS_counter_u64(service_request_counter, "service_request_counter", "service_request_counter");

// 为metrics添加一个label
ADD_COUNTER_LABEL(service_request_counter, "method");

void ServiceImpl::UpdateUserInfo(...) {
    // ...

    // 更新标签为UpdateUserInfo的bvar数据(+1)
    ADD_METRICS_COUNTER(service_request_counter, "UpdateUserInfo"); // 等价于 ADD_METRICS_COUNTER_VAL(service_request_counter, 1, "UpdateUserInfo");
}

void ServiceImpl::Test(...) {
    // ...

    // 更新标签为Test的bvar数据(+1)
    ADD_METRICS_COUNTER(service_request_counter, "Test"); // 等价于 ADD_METRICS_COUNTER_VAL(service_request_counter, 1, "Test");
}
```
服务启动并有请求访问后，访问ip:port/metrics可以得到如下输出:
```yaml
# TYPE service_request_counter gauge
service_request_counter{method="Test"} 2381312
service_request_counter{method="UpdateUserInfo"} 2381038
```
在prometheus配置如下抓取任务：
```yaml
scrape_configs:
  - job_name: "mc_server_metrics"
    metrics_path: "metrics"
    file_sd_configs:
    - refresh_interval: 10s
      files:
        - /etc/prometheus/instance.json
```
配合Grafana可以更方便的看到整个服务下每个接口的qps(PromQL: `sum(rate(service_request_counter{service_name="$service_name"}[1m])) by (method)`)：
![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/4d1cb0659c234bcc9778a6ab1ece5f92~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1287&h=447&s=53760&e=png&b=171a1e)

### 多维延迟统计类——MetricsLatencyRecorder
`MetricsLatencyRecorder`是多维延迟统计类，可以统计不同分位值的延迟数据，实现方式和使用方式和`MetricsCountRecorder`类似。同样以统计brpc_test服务每个接口延迟为例:
```c++
// 定义metrics(request_latency_recorder)
DEFINE_METRICS_latency(request_latency_recorder, "request_latency_recorder", "request_latency_recorder");

void ServiceImpl::UpdateUserInfo(...) {
    butil::Timer timer(butil::Timer::TimerType::STARTED);
    // ...

    // 模拟延迟
    bthread_usleep(10000 + (rd() % 103907));
    timer.stop();

    // 设置metrics延迟数据
    SET_METRICS_LATENCY(request_latency_recorder, timer.m_elapsed(), "UpdateUserInfo");

    // ...
}

void ServiceImpl::Test(...) {
    butil::Timer timer(butil::Timer::TimerType::STARTED);
    // ...

    // 模拟延迟
    bthread_usleep(10000 + (rd() % 103907));
    timer.stop();

    // 设置metrics延迟数据
    SET_METRICS_LATENCY(request_latency_recorder, timer.m_elapsed(), "Test");

    // ...
}

```
Grafana展示效果如下(PromQL: `avg(request_latency_recorder{service_name="$service_name", quantile="0.99"}) by (method)`)：
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/748dad88a8ae4015806a548e814c9240~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=1264&h=386&s=54753&e=png&b=181b1f)

### Server侧请求数统计
通过在brpc::Server类中添加两个`MetricsCountRecorder<uint64_t>`的实例_server_request_total_counter和_server_request_error_counter，分别用于服务端收到请求总数及响应出错数，两个counter带有标签分别如下:
```c++
// brpc::Server::Server(...)
_server_request_total_counter = new bvar::MetricsCountRecorder<uint64_t>(
    "server_request_total_counter", "count total request num at server-side");
_server_request_total_counter->set_metrics_label("service");       // 当前请求访问服务service名（类名）
_server_request_total_counter->set_metrics_label("method");        // 当前请求访问方法名
_server_request_total_counter->set_metrics_label("protocol");      // 当前请求协议(http、baidu_std等)
_server_request_total_counter->set_metrics_label("fsvr_name");     // 上游服务名(即客户端注册到服务中心的名字)

_server_request_error_counter = new bvar::MetricsCountRecorder<uint64_t>(
    "server_request_error_counter", "count error request num at server-side");
_server_request_error_counter->set_metrics_label("service");
_server_request_error_counter->set_metrics_label("method");
_server_request_error_counter->set_metrics_label("protocol");
_server_request_error_counter->set_metrics_label("fsvr_name");
_server_request_error_counter->set_metrics_label("error_info");   // 错误码及对应错误描述 
```

在每个协议的ProcessXXXRequest函数中调用`ServerPrivateAccessor`的`AddTotalCounter`函数及`AddErrorCounter`函数来更新上述两个counter的计数。有了这个计数器，可以通过PromeQL方便的统计统计某个服务内各个接口的响应QPS，如统计brpc_test服务中各个接口过去1小时内针对不同上游服务的响应QPS:
```c++
sum(rate(server_request_total_counter{server_name="brpc_test"}[1h])) by (fsvr_name, method)
```

### Client请求数统计
客户端请求总数是在brpc::Channel类中新增了两个`MetricsCountRecorder<uint64_t>`的实例_client_request_total_counter和_client_request_error_counter，分别用于统计当前服务作为客户端请求的总数和请求出错数，两个counter带有标签分别如下:
```c++
_client_request_total_counter = std::make_shared<bvar::MetricsCountRecorder<uint64_t>>(
        "client_request_total_counter", "count total request num at client-side");
_client_request_total_counter->set_metrics_label("tsvr_name", tsvr_name);   // 下游服务名(即访问模板服务名）
_client_request_total_counter->set_metrics_label("protocol", protocol);     // 请求协议(http、baidu_std等)
_client_request_total_counter->set_metrics_label("method");                 // 请求接口(方法)名

_client_request_error_counter = std::make_shared<bvar::MetricsCountRecorder<uint64_t>>(
    "client_request_error_counter", "count error request num at client-side");
_client_request_error_counter->set_metrics_label("tsvr_name", tsvr_name);
_client_request_error_counter->set_metrics_label("protocol", protocol);
_client_request_error_counter->set_metrics_label("method");
_client_request_error_counter->set_metrics_label("tinstance");              // 下游实例地址(及当前lb选出的目标server实例ip:port)
_client_request_error_counter->set_metrics_label("error_info");             // 错误码及对应错误描述
```
由于brpc中客户端发起请求的过程是在`Controller::IssueRPC`方法中完成，并且请求返回后是也是在`Controller::OnVersionedRPCReturned`方法中处理，所以对client侧计数器的更新应该放到Controller中完成，因此我们在brpc::Controller中也添加了两个对应的`MetricsCountRecorder<uint64_t>`指针`Controller::_client_request_total_counter`和`Controller::_client_request_error_counter`，在实际发起请求前我们将Channel中已经初始化的counter指针赋值给Controller中对应的指针，在Controller中去更新计数器:
```c++
// Channel::CallMethod(...)
if (bvar::FLAGS_enable_rpc_metrics) {
    // cntl get counters from channel
    cntl->_client_request_total_counter = _client_request_total_counter;
    cntl->_client_request_error_counter = _client_request_error_counter;
    cntl->_client_request_latency_recorder = _client_request_latency_recorder;
}
```
同样，有了这两个计数器我们可以通过PromeQL方便的统计当前服务作为客户端访问下游服务各接口的QPS:
```c++
sum(rate(client_request_total_counter{server_name="brpc_test"}[1h])) by (tsvr_name, method)
```

### Server侧响应延迟统计
服务端响应延迟通过在brpc::MethodStatus类中新增一个`bvar::MetricsLatencyRecorder`实例指针_server_response_latency_recorder，初始化如下:
```c++
// MethodStatus::Expose(...)
_server_response_latency_recorder = std::make_shared<bvar::MetricsLatencyRecorder>(
            "server_response_latency_recorder",
            "statistic response latency at server-side");                      
_server_response_latency_recorder->set_metrics_label("fsvrname");   // 上游服务名
_server_response_latency_recorder->set_metrics_label("service");    // 当前请求访问service名(类名)
_server_response_latency_recorder->set_metrics_label("method");     // 当前请求访问接口名
_server_response_latency_recorder->set_metrics_label("proto");      // 请求协议
```
注意上述recorder的初始化是放在`brpc::MethodStatus::Expose`方法中完成的，初始化时机及流程是brpc::Server启动时调用`Server::StartInternal`函数，进而调用`Server::UpdateDerivedVars`，然后在调用`brpc::MethodStatus::Expose`：
```c++
// Server::UpdateDerivedVars(...)
for (MethodMap::iterator it = server->_method_map.begin();
        it != server->_method_map.end(); ++it) {
    // Not expose counters on builtin services.
    if (!it->second.is_builtin_service) {
        mprefix.resize(prefix.size());
        mprefix.push_back('_');
        bvar::to_underscored_name(&mprefix, it->second.method->full_name());
        it->second.status->Expose(mprefix);  // here `_server_response_latency_recorder` inited.
    }
}
```
计数器更新则是在各个协议的SendXXXResponse函数中通过RAII机制在ConcurrencyRemover对象的析构函数中触发`MethodStatus::LatencyRec`进行。
通过server_response_latency_recorder统计当前服务各个接口响应延迟的PromeQL如下：
```c++
avg(server_response_latency_recorder{server_name="brpc_test", quantile="0.99"} / 1000) by (service, method)
```

### Client请求延迟统计
client侧请求延迟统计和client侧请求总数统计实现方法完全一致，分别在Channel及Controller中添加`bvar::MetricsLatencyRecorder`对象指针_client_request_latency_recorder，在`Controller::OnVersionedRPCReturned`中rpc结束时更新计数器。
```c++
_client_request_latency_recorder = std::make_shared<bvar::MetricsLatencyRecorder>(
        "client_request_latency_recorder", "statistic request latency at client-side");
_client_request_latency_recorder->set_metrics_label("tsvr_name", tsvr_name);  // 请求服务名
_client_request_latency_recorder->set_metrics_label("protocol", protocol);    // 请求协议
_client_request_latency_recorder->set_metrics_label("method");                // 请求接口名
_client_request_latency_recorder->set_metrics_label("tinstance");             // 下游响应实例地址(ip:port)
```
统计最为客户端请求延迟的PromeQL如下:
```c++
avg(client_request_latency_recorder{server_name="brpc_test", quantile="0.99"} / 1000) by (service, method)
```

### 整体监控示例效果图
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/51e2dba6305a45ee885c3a5bd0c96c5b~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=2560&h=1288&s=353396&e=png&b=191c20)

## 6. 客户端主动容灾
brpc框架本身支持负载均衡路由算法容灾依赖目标server心跳检测，不具备主动容灾能力，即一个后端连接如果断掉且重连失败，会从名字里摘掉。心跳检测可以解决一部分问题，但是有其局限性。首先，心跳一般都是每个一段时间(比如每隔几秒)检测一次，因此当某个目标ip异常时，通过心跳剔除目标会有一定延迟，期间可能仍然会有大量rpc访问失败；其次，当后端某个ip cpu负载高，网络抖动，丢包率上升等情况下，一般心跳检查是能通过的，但是此时我们判断该ip是处于一种异常状态，需要降低访问权重来进行调节；再则，目标ip一切正常，只是其中某个端口的服务异常，这种情况下我们只需要对特定ip:port的下游进行降级或熔断，而不需要屏蔽整个ip。这就是我们要做的主动容灾，主要分为以下三部分：

1. rpc调用结果上报(已实现)
2. rpc调用结果收集(已实现)
3. 熔断降级策略生成(已实现)
4. 策略应用(待实现)

### RPC调用结果上报——LbStatClient
目前mc-brpc已经在brpc框架内支持将rpc调用上报至本机的NameAgent进程。上报功能由<font color=#00ffff>LbStatClient</font>(core/lb_stat/lb_stat_client.h)完成，出于代码隔离考虑，brpc源码不直接include基础库(core)代码，mc-brpc在brpc中新增了个<font color=#00ffff>BaseLbStat</font>抽象类(brpc/policy/base_lb_stat.h)，再由<font color=#00ffff>LbStatClient</font>继承<font color=#00ffff>BaseLbStat</font>，重写`BaseLbStat::LbStatReport`进行rpc调用结果上报。 
 
MCServer会在启动时完成LbStatClient的初始化及全局注册：
```c++
void MCServer::Start(bool register_service) {
    // ...

    // init lb stat
    server::lb_stat::LbStatClient::GetInstance()->Init();

    // ...
}

void LbStatClient::Init() {
    // register lb stat
    BaseLbStatExtension()->RegisterOrDie(LB_STAT_CLIENT, this);

    // start report thread
    _report_thread = std::thread(&LbStatClient::RealReport, this);
}

```
上报逻辑预埋在rpc调用结束`brpc::Controller::OnVersionedRPCReturned`处：
```c++
void Controller::OnVersionedRPCReturned(const CompletionInfo& info,
                                        bool new_bthread, int saved_error) {
    // ...

    // lb report 
    brpc::policy::BaseLbStat *lb_stat = brpc::policy::BaseLbStatExtension()->Find(LB_STAT_CLIENT);
    if (lb_stat) {
        lb_stat->LbStatReport(_to_svr_name, _remote_side, _error_code, info.responded, latency / 1000);
    }

    // ...
}

int LbStatClient::LbStatReport(
    const std::string& service_name,
    const butil::EndPoint& endpoint,
    int ret,
    bool responsed,
    int cost_time) {
    if (endpoint.port <= 0 || endpoint.port > 65535 || !memcmp(butil::ip2str(endpoint.ip).c_str(), "0.0.0.0", 7)) {
        return 0;
    }
    name_agent::LbStatInfo info;
    info.mutable_endpoint()->set_ip(butil::ip2str(endpoint.ip).c_str());
    info.mutable_endpoint()->set_port(endpoint.port);
    info.set_service_name(service_name);
    if ((!responsed && ret)) {
        // no response and ret!=0, take as net err or EREJECT/ELIMIT/ELOGOFF take as sys err
        info.set_fail_cnt(1);
        info.set_fail_net_cnt(1);
        LOG(INFO) << "report net err, service_name:" << service_name << ", " << butil::endpoint2str(endpoint).c_str()
                  << ", ret:" << ret << ", responsed:" << responsed << ", cost_time:" << cost_time << " ms";
    } else if (responsed && ret) {
        info.set_fail_cnt(1);
        info.set_fail_logic_cnt(1);
        LOG(INFO) << "report logic error, service_name:" << service_name << ", "
                  << butil::endpoint2str(endpoint).c_str() << ", ret:" << ret << ", cost_time:" << cost_time << " ms";
    } else {  // not report logic err yet
        info.set_succ_cnt(1);
    }
    info.set_used_ms(cost_time);

    ServerStats* stats = GetStatsByAddr(service_name, info.endpoint().ip(), info.endpoint().port());

#define _ADD_COUNTER(field) __sync_fetch_and_add(&stats->field, info.field())
    _ADD_COUNTER(succ_cnt);
    _ADD_COUNTER(fail_cnt);
    _ADD_COUNTER(fail_net_cnt);
    _ADD_COUNTER(fail_logic_cnt);
    _ADD_COUNTER(used_ms);
#undef _ADD_COUNTER
    return 0;
}
```                                            
为避免上报过程对rpc本身过程造成性能影响，<font color=#00ffff>LbStatClient</font>(core/lb_stat/lb_stat_client.h)默认情况下会将调用结果先原子的写入本地缓存进行聚合，再由一个独立的上报线程周期性(默认200ms，执行逻辑为`LbStatClient::RealReport`)的将聚合结果通过unix_socket上报至本机brpc_name_agent进程(由NameAgent进程的LbStatSvr模块进程处理, 具体实现请看源码):
```c++
void LbStatClient::RealReport() {
    LOG(INFO) << "Lb report thread start...";
    while (!_is_asked_to_stop) {
        DoLbReport();
        usleep(FLAGS_report_interval_ms * 1000);
    }
    LOG(INFO) << "Lb report thread finished...";
}

int LbStatClient::DoLbReport() {
    std::vector<ServerStats> stats;
    CollectStats(stats);

    size_t idx(0), pos(0), round(10);
    do {
        if (round == 0) {
            LOG(WARNING) << "[*] reach the limit.";
            break;
        }
        --round;

        name_agent::LbStatReportReq req;
        for (; idx < stats.size() && idx < pos + FLAGS_report_batch_num; ++idx) {
            const ServerStats& s = stats[idx];

            name_agent::LbStatInfo* info = req.add_infos();
            info->set_service_name(s.service_name);
            info->mutable_endpoint()->set_ip(s.ip);
            info->mutable_endpoint()->set_port(s.port);
#define _COPY_FIELD(field) info->set_##field(s.field)
            _COPY_FIELD(succ_cnt);
            _COPY_FIELD(fail_cnt);
            _COPY_FIELD(fail_net_cnt);
            _COPY_FIELD(fail_logic_cnt);
            _COPY_FIELD(used_ms);
#undef _COPY_FIELD

            LOG(DEBUG) << "real lb_report, info, service_name:" << info->service_name() << ", ip:" << s.ip
                       << ", port:" << s.port << ", succ_cnt:" << info->succ_cnt() << " , fail_cnt:" << info->fail_cnt()
                       << ", used_ms:" << info->used_ms();
        }

        if (req.infos_size() > 0) {
            NameAgentClient::GetInstance()->LbStatReport(req);
        }

        pos = idx;
    } while (idx < stats.size());

    return 0;
}
```

上报至NameAgent的信息包含以下内容(目前只用到了前四个字段):
```protobuf
message LbStatInfo {
	string endpoint   = 1;     // rpc下游实例ip:port
	string service_name = 2;   
	uint32 succ_cnt      = 3;  // 该周期内目标节点请求成功数
	uint32 fail_cnt      = 4;  // 该周期内目标节点失败数
	uint32 fail_net_cnt  = 5;  // 该周期内目标节点网络异常数
	uint32 fail_logic_cnt= 6;  // 该周期内目标节点逻辑异常数
	uint32 used_ms       = 7;  // 该周期内对目标节点rpc访问总耗时
}
```
### RPC调用结果收集——LbStatSvr
NameAgent的LbStatSvr模块负责收集RPC调用结果，它使用了两个map做双buf设计，收到客户端上报来的rpc调用结果后，会将信息原子的写入到当前统计周期的map：
```c++
#define ADD_FILEDS(stat, field) __sync_fetch_and_add(&(stat->field), info.field())

int LbStatSvr::LbAddStat(const name_agent::LbStatInfo& info) {
    std::string key = butil::string_printf("%s:%d", info.endpoint().ip().c_str(), info.endpoint().port());

    ServerStats* p_stat = nullptr;
    {
        BAIDU_SCOPED_LOCK(_mutex);
        auto it = _cur_svr_stat->find(key);
        if (it == _cur_svr_stat->end()) {
            p_stat = new ServerStats;
            p_stat->ip = info.endpoint().ip();
            p_stat->port = info.endpoint().port();
            p_stat->service_name.assign(info.service_name());
            (*_cur_svr_stat)[key] = p_stat;
        } else {
            p_stat = it->second;
        }
    }

    ADD_FILEDS(p_stat, succ_cnt);
    ADD_FILEDS(p_stat, fail_cnt);
    ADD_FILEDS(p_stat, fail_net_cnt);
    ADD_FILEDS(p_stat, fail_logic_cnt);
    ADD_FILEDS(p_stat, used_ms);

    return 0;
}
```
另外LbStatSvr还额外启动一个线程(StrategyThread)周期性的调用`LbStatSvr::GenerateStrategy()`根据上周期的上报数据生成熔断降级策略，每次生成策略的时候需要将当前写数据的map置换出来，置换的时候，直接更新当前正在使用的map的指针，所以无需加锁。 置换出本周期map后，基于里面的rpc上报结果产生对应的熔断降级策略（如通过率多少），相应的策略会以ip:port为key写入本机的共享内存里，供其他业务在做路由算法的时候使用(具体策略生成实现见下一小节):
```c++
void LbStatSvr::SwapSvrStat() {
    BAIDU_SCOPED_LOCK(_mutex);
    ++_inuse_idx;
    _inuse_idx = _inuse_idx % 2;
    _cur_svr_stat = &_svr_stat_list[_inuse_idx];
}

void LbStatSvr::GenerateStrategy() {
    LbStatInfoMap* cur_stat = _cur_svr_stat;
    uint32_t size = cur_stat->size();
    SwapSvrStat();
    usleep(20 * 1000);  // wait a moment, so that all clients have switch to new LbStatInfoMap

    butil::Timer timer(butil::Timer::STARTED);

    const StrategyGenerator* p_strategy = StrategyGenerator::GetRegisteredStrategy();
    p_strategy->UpdateStrategy(*cur_stat);

    timer.stop();
    uint32_t cost_ms = timer.m_elapsed();
    LOG_EVERY_N(INFO, 10) << "lb strategy updated, cost_ms:" << cost_ms << ", size:" << size;
    if (cost_ms > FLAGS_strategy_generate_interval_ms - 100) {
        // TODO: alarm，cost too much time, optimization needed.
    }
}
```
### 熔断降级策略生成
上节提到NameAgent的LbStatSvr会额外启动一个<font color=#00ffff>StrategyThread</font>专门用于生成熔断降级策略，<font color=#00ffff>StrategyThread</font>执行内容如下:
```c++
void LbStrategyThread() {
    // init strategy shm as svr
    int ret = StrategyShm::GetInstance()->Init(0);
    if (ret) {
        LOG(ERROR) << "StrategyShm::GetInstance()->Init as svr fail, ret:" << ret;
        exit(-1);
    }

// register lb strategy
#ifdef USE_MC_STRATEGY_GENERATOR
    StrategyGenerator::RegisterStrategy(new McStrategyGenerator());
    LOG(INFO) << "using McStrategyGenerator...";
#else
    StrategyGenerator::RegisterStrategy(new DefaultStrategyGenerator());
    LOG(INFO) << "using DefaultStrategyGenerator...";
#endif

    // loop, generate lb startegy periodically
    LOG(INFO) << "lb strategy thread start...";
    while (1) {
        LbStatSvr::GetInstance()->GenerateStrategy();
        usleep(FLAGS_strategy_generate_interval_ms * 1000);
    }
}
```
* 首先<font color=#00ffff>StrategyThread</font>会申请一块本机的共享内存，用于存放生成的策略内容，并提供给其它进程做rpc路由算法时使用。之所以采用共享内存的方式通信是因为这样效率最高，我们希望客户端在使用熔断降级策略的时候不会对其rpc本身的性能造成太大影响，因此读取熔断降级策略的耗时至少要比rpc本身的耗时低一个数量级
* 注册策略生成器<font color=#00ffff>StrategyGenerator</font>，它用于根据当前周期内rpc上报结果生成或调整熔断降级策略，并写入共享内存；目前mc-brpc提供了两种熔断降级策略生成器算法：<font color=#00ffff>DefaultStrategyGenerator</font>(默认使用)和<font color=#00ffff>McStrategyGenerator</font>，用户也可以继承StrategyGenerator并重写`StrategyGenerator::UpdateStrategy`方法实现自定义的生成算法。
* 周期性(默认1s)调用`LbStatSvr::GenerateStrategy()`处理上报结果并生成策略(生成策略则是通过上一步中注册的StrategyGenerator完成)。

#### 策略共享内存管理——StrategyShm
```c++
class StrategyShm {
    friend DefaultStrategyGenerator;
    friend McStrategyGenerator;

public:
    StrategyShm();
    ~StrategyShm();
    static StrategyShm* GetInstance();

    /**
     * cli and nameagent all need do init with on thread, cli can do this in stat cli init
     * role: 0(name_agent)  1(cli)
     */
    int Init(int role);

    int TryAttatchShm();       // attach not create
    int AttachAndCreateShm();  // attach, if no do create
    int GetStrategy(const char* ip_str, short port, StrategyShmInfo*& info);
#ifdef LOCAL_TEST
    void PrintStrategy();
#endif
private:
    char* _shm_head;
    char* _shm_body0;
    char* _shm_body1;
    int _body_len;
    butil::atomic<bool> _init_done;

    MultiLevelHash<std::string, StrategyShmInfo>* shm_hash0;
    MultiLevelHash<std::string, StrategyShmInfo>* shm_hash1;
};
```

<font color=#00ffff>StrategyShm</font>负责管理存放策略结果的共享内存，不管是NameAgent进程还是其他业务进程，启动时都会通过`StrategyShm::Init()`初始化一块共享内存，区别在于NamaAgent进程是创建一块内存，而业务进程是attach到NameAgent创建的共享内存块上；创建完共享内存后，需要对内存块做些初始化工作，这里<font color=#00ffff>StrategyShm</font>采用了多阶hash结构<font color=#00ffff>MultiLevelHash</font>管理共享内存块，使用多阶hash有以下好处：
1. 查找性能非常高，O(1)，由于冲突后的阶数相对固定，可以认为冲突时的性能也是O(1)
2. 解决冲突的方法是跳下一阶，无需动态开内存，可以完美契合共享内存
<center>
    
![multi_hash.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/2d2f717720ad44f68d6f8379c2148148~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=634&h=378&s=18548&e=png&b=fefefe)
</center>

上图即为多阶hash的结构。为了尽量保证查找性能，我们控制Level只有4层。第一层元素个数为N1(25万)，N1:N2:N3:N4=8:4:2:1, 单个策略元素由`StrategyShmInfo`表示，大小约为50 Bytes(只用了22 Bytes, 其他28 Bytes作为保留字段)，所以整块多阶hash的内存为50*(250000+125000+62500+31250)约为24MB。目标是该多阶hash可以存储25万级的key，如果写入的时候，4阶都用满了，此时策略写入失败，只能告警出来，理论上只要总key量在25 万以内，一般不会出现。   

这里策略使用的共享内存是典型的"一写多读"场景，写是超低频的，由StrategyThread周期性进行更新写入；读是高频的，由其他业务进程发起rpc做路由算法时访问。简单来做，加锁可以解决。但是由于写的时候涉及大量item的更新，一把大锁会导致此时全部读请求都阻塞一段较长时间，如果每个item都用一把锁，则又过于浪费。所以这里多阶hash和共享内存结合的时候，也使用了双buf的设计，总共申请约50M的内存，并在此内存块上初始化两条<font color=#00ffff>MultiLevelHash</font>:
<center>
    
![image2018-8-29_10-17-49.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c5ce3218864c4d4182a825462b5a5fef~tplv-k3u1fbpfcp-jj-mark:0:0:0:0:q75.image#?w=527&h=50&s=2236&e=png&b=fdfdfd)
</center>

整块共享内存分为3块：Head、M_hash0、M_hash1；其中M_hash0和M_hash1都是前面提到的多阶hash结构，而head大约20字节，里面主要有cur_hashtable_idx，只能是0或者1，分别表示使用当前使用M_hash0或者M_hash1，全部的hash查找都是基于cur_hashtable_idx找到对应的多阶hash表。

从而在有新的一批策略需要写入的时，只需要先把`(cur_hashtable_id + 1) % 2`对应的多阶hash表先更新，然后再修改cur_hashtable_id的值，就完成了双表切换。全程避免了用锁。双表切换完之后，写线程等待一个足够长的时间，比如100ms, 再把老的cur_hashtable_id对应的那张表内容重置以便下次写入。

#### 策略生成类——StrategyGenerator
```c++
class StrategyGenerator {
public:
    static void RegisterStrategy(const StrategyGenerator* p_strategy) {
        brpc::Extension<const StrategyGenerator>::instance()->RegisterOrDie(STATEGY_NAME, p_strategy);
    }

    static const StrategyGenerator* GetRegisteredStrategy() {
        return brpc::Extension<const StrategyGenerator>::instance()->Find(STATEGY_NAME);
    }

    /**
     * UpdateStrategy is in charge of doing following works:
     * 1. scan and read from cur_hash, merge changes and rewrite into backup_hashtable
     * 2. then, switch to use backup_hashtable
     * 3. reset origin cur_hashtable and clear stat_map(do not forget)
     */
    virtual int UpdateStrategy(std::unordered_map<std::string, ServerStats*>& stat_map) const = 0;
};
```
<font color=#00ffff>StrategyGenerator</font>是一个抽象类，提供了注册和获取当前使用的StrategyGenerator实现的方法，用户需要继承StrategyGenerator并重写其`UpdateStrategy`方法以实现自己的熔断降级策略更新、生成算法。目前mc-brpc提供了两种StrategyGenerator的实现：<font color=#00ffff>DefaultStrategyGenerator</font>和<font color=#00ffff>McStrategyGenerator</font>。  

<font color=#00ffff>DefaultStrategyGenerator</font>更新下游权重的思路比较简单，它直接根据上个统计周期内各个目标server(ip:port)的成功率来调整其访问占比权重，即某个server上周期内访问成功率为100，则设置其当前周期内访问权重为100；若上周期内成功率为80%，则设置当前周期内访问权重为80，算法实现为：
1. 首先找到当前客户端读取的多阶hash策略表(cur_hash)，以及本次更新应写入的多阶hash(backup_hash)
2. 扫描cur_hash，对于其中每个有效的策略item(`key_len != 0`)，如果在当前统计周期统计数据stat_map找到其key，则对该item做更新后插入到backup_hash；反之，如果在stat_map中未找到其key，可看该策略生成时间是否超过了策略最大生存时间(通过gflags参数max_lb_strategy_live_time_s可指定， 默认30s)，否则原样插入backup_hash，是则丢弃
3. 针对在stat_map中出现却不在cur_hash中的ip:port(即首次出现的)，为其生成对应策略item并插入backup_hash
4. 切换cur_hash和backup_hash前后台关系，即更新head中cur_shmboard_idx(`head->cur_shmboard_idx = (head->cur_shmboard_idx + 1) % 2`), 之后客户端读取策略信息即从backup_hash读取
5. sleep一小会确保旧的hash（即上面的cur_hash）没再被使用，并重置其数据
```c++
int DefaultStrategyGenerator::UpdateStrategy(std::unordered_map<std::string, ServerStats*>& stat_map) const {
    StrategyShm* shm = StrategyShm::GetInstance();
    if (!shm || !shm->_shm_head) {
        LOG(ERROR) << "fatal error: _shm_head is nullptr";
        return -1;
    }
    StrategyShmHead* head = (StrategyShmHead*)shm->_shm_head;

    MultiLevelHash<std::string, StrategyShmInfo>* volatile cur_hash =
        (head->cur_shmboard_idx ? shm->shm_hash1 : shm->shm_hash0);
    MultiLevelHash<std::string, StrategyShmInfo>* volatile backup_hash =
        (head->cur_shmboard_idx ? shm->shm_hash0 : shm->shm_hash1);

    uint32_t now = butil::gettimeofday_s();
    for (uint32_t level = 0; level < cur_hash->GetMaxLevelNum(); ++level) {
        unsigned int item_num = 0;
        StrategyShmInfo* raw_hash_table = nullptr;
        int ret = cur_hash->GetRawHashTable(level, raw_hash_table, item_num);
        if (ret || !raw_hash_table) {
            LOG(ERROR) << "failed to get raw hash table!";
            return -2;
        }

        for (uint32_t k = 0; k < item_num; ++k) {
            StrategyShmInfo* shm_info = raw_hash_table + k;
            if (shm_info->key_len == 0) {
                continue;
            }

            butil::EndPoint ep;
            if (shm_info->Key2EndPoint(&ep) != 0) {
                continue;
            }
            std::string key(butil::endpoint2str(ep).c_str());

            auto it = stat_map.find(key);
            if (it != stat_map.end()) {
                // update pass_rate by stat
                uint32_t succ_cnt = it->second->succ_cnt;
                uint32_t total_req_cnt = it->second->succ_cnt + it->second->fail_cnt;
                stat_map.erase(it);
                if (total_req_cnt != 0) {
                    double cur_pass_rate = 100 * (double)succ_cnt / total_req_cnt;
                    StrategyShmInfo* backup_shm_info = backup_hash->Insert(*shm_info);
                    if (backup_shm_info) {
                        backup_shm_info->cur_req_cnt = total_req_cnt;
                        backup_shm_info->pass_rate = (short)cur_pass_rate;
                        backup_shm_info->strategy_time = now;
                        continue;
                    }
                }
            }

            // when it == stat_map.end()
            if (shm_info->strategy_time + FLAGS_max_lb_strategy_live_time_s > now) {
                backup_hash->Insert(*shm_info);
            }
        }
    }

    // handle those not found in cur_hashtable
    for (auto it = stat_map.begin(); it != stat_map.end();) {
        uint32_t total_req_cnt = it->second->succ_cnt + it->second->fail_cnt;
        if (total_req_cnt != 0) {
            double cur_pass_rate = 100 * (double)it->second->succ_cnt / total_req_cnt;
            StrategyShmInfo shm_info;
            shm_info.MakeKey(it->second->ip.c_str(), it->second->port);
            shm_info.pass_rate = (short)cur_pass_rate;
            shm_info.cur_req_cnt = total_req_cnt;
            shm_info.strategy_time = now;
            backup_hash->Insert(shm_info);
        }
        stat_map.erase(it++);
    }

    // swicth to use new hashtable, wait 100ms then reset original cur_hashtable
    head->cur_shmboard_idx = (head->cur_shmboard_idx + 1) % 2;
    usleep(100 * 1000);
    cur_hash->Reset();

    return 0;
}
```

由于对多阶hash的更新是在独立线程(StrategyThread)内完成，不存在并发多点写入情况，且使用了双buf设计，上述整个更新过程都是lock free的。

<font color=#00ffff>McStrategyGenerator</font>更新下游访问权重的流程和步骤和<font color=#00ffff>DefaultStrategyGenerator</font>完全一致，只是计算通过率的方式不一样。McStrategyGenerator根据上个统计周期通过率、当前上报成功率即当前访问QPS来计算当前通过率，具体的策略算法如下：
* 上个统计周期通过率为0，如果当前成功率大于80%，则将当前通过率设置为15%(可配置)
* 上个统计周期通过率不为0，则根据../conf/lb_strategy_conf.json中的规则来更新当前通过率，lb_strategy_conf.json配置样例如下：
```json
{
    "200": {
        "30": -80,
        "80": -30,
        "99": -5,
        "100": 80 
    },
    "500": {
        "50": -80,
        "80": -50,
        "99": -10,
        "100": 70
    },
    "1000": {
        "70": -80,
        "99": -20,
        "100": 50
    },
    ...
}
```
配置中第一层key为当前上报数(QPS), 第二层key为当前上报数据中成功率，value为当前通过率应该在上一统计周期通过率基础上增加或者减少(负数表示减少)的百分比。以配置的第一项为例稍加说明: 
* 即在当前上报数(QPS)小于等于200的时候：
* * 如果当前上保数据中成功率小于等于30%，则当前通过率在上个统计周期通过率基础上减少80%（调整后最低0）
* * 如果当前上保数据中成功率大于30%但小于等于80%，则当前通过率在上个统计周期通过率基础上减少30%（调整后最低0）
* * 如果当前上保数据中成功率大于80%但小于等于99%，则当前通过率在上个统计周期通过率基础上减少5%（调整后最低0）
* * 如果当前上保数据中成功率为100%，则当前通过率在上个统计周期通过率基础上增加80%（调整后最大为100%）
  
从配置可以看出，再上报QPS相同的情况下，成功率越低，通过率下降越快；在成功率相同情况下，上报QPS越高，通过率下降越快，恢复越慢。这里配置的阈值都是固定写死在配置文件的，我们也可以通过一下AIOPS的手段动态调整各项阈值以获取更好的效果。
```c++
int McStrategyGenerator::UpdateStrategy(std::unordered_map<std::string, ServerStats*>& stat_map) const {
    StrategyShm* shm = StrategyShm::GetInstance();
    if (!shm || !shm->_shm_head) {
        LOG(ERROR) << "fatal error: _shm_head is nullptr";
        return -1;
    }
    StrategyShmHead* head = (StrategyShmHead*)shm->_shm_head;

    MultiLevelHash<std::string, StrategyShmInfo>* volatile cur_hash =
        (head->cur_shmboard_idx ? shm->shm_hash1 : shm->shm_hash0);
    MultiLevelHash<std::string, StrategyShmInfo>* volatile backup_hash =
        (head->cur_shmboard_idx ? shm->shm_hash0 : shm->shm_hash1);

    uint32_t now = butil::gettimeofday_s();
    for (uint32_t level = 0; level < cur_hash->GetMaxLevelNum(); ++level) {
        unsigned int item_num = 0;
        StrategyShmInfo* raw_hash_table = nullptr;
        int ret = cur_hash->GetRawHashTable(level, raw_hash_table, item_num);
        if (ret || !raw_hash_table) {
            LOG(ERROR) << "failed to get raw hash table!";
            return -2;
        }

        for (uint32_t k = 0; k < item_num; ++k) {
            StrategyShmInfo* shm_info = raw_hash_table + k;
            if (shm_info->key_len == 0) {
                continue;
            }

            butil::EndPoint ep;
            if (shm_info->Key2EndPoint(&ep) != 0) {
                continue;
            }
            std::string key(butil::endpoint2str(ep).c_str());

            auto it = stat_map.find(key);
            if (it != stat_map.end()) {
                uint32_t succ_cnt = it->second->succ_cnt;
                uint32_t total_req_cnt = it->second->succ_cnt + it->second->fail_cnt;
                stat_map.erase(it);
                if (total_req_cnt == 0) {
                    if (shm_info->strategy_time + FLAGS_max_lb_strategy_live_time_s > now) {
                        backup_hash->Insert(*shm_info);
                    }
                } else {
                    StrategyShmInfo* backup_shm_info = backup_hash->Insert(*shm_info);
                    if (!backup_shm_info) {
                        continue;
                    }
                    backup_shm_info->cur_req_cnt = total_req_cnt;
                    backup_shm_info->strategy_time = now;

                    double cur_pass_rate = 100 * (double)succ_cnt / total_req_cnt;
                    if (shm_info->pass_rate == 0) {
                        if (cur_pass_rate > 80) {
                            backup_shm_info->pass_rate += 15;
                        }
                    } else {
                        auto it1 = _strategy_config.lower_bound(total_req_cnt);
                        if (it1 == _strategy_config.end()) {
                            LOG(WARNING) << "startegy config not found, req_cnt:" << total_req_cnt;
                            continue;
                        }

                        auto it2 = it1->second.lower_bound((uint32_t)cur_pass_rate);
                        if (it2 == it1->second.end()) {
                            LOG(WARNING) << "startegy config not found, req_cnt:" << total_req_cnt
                                         << ", pass_rate:" << cur_pass_rate;
                            continue;
                        }
                        backup_shm_info->pass_rate *= ((100 + it2->second) / 100.0);
                        backup_shm_info->pass_rate = std::min(backup_shm_info->pass_rate, (short)100);
                        backup_shm_info->pass_rate = std::max(backup_shm_info->pass_rate, (short)0);
                    }
                }
            } else {
                if (shm_info->strategy_time + FLAGS_max_lb_strategy_live_time_s > now) {
                    backup_hash->Insert(*shm_info);
                }
            }
        }
    }

    // handle those not found in cur_hashtable
    for (auto it = stat_map.begin(); it != stat_map.end();) {
        uint32_t total_req_cnt = it->second->succ_cnt + it->second->fail_cnt;
        if (total_req_cnt != 0) {
            double cur_pass_rate = 100 * (double)it->second->succ_cnt / total_req_cnt;
            StrategyShmInfo shm_info;
            shm_info.MakeKey(it->second->ip.c_str(), it->second->port);
            shm_info.pass_rate = (short)cur_pass_rate;
            shm_info.cur_req_cnt = total_req_cnt;
            shm_info.strategy_time = now;
            backup_hash->Insert(shm_info);
        }
        stat_map.erase(it++);
    }

    // swicth to use new hashtable, wait 20ms then reset original cur_hashtable
    head->cur_shmboard_idx = (head->cur_shmboard_idx + 1) % 2;
    usleep(100 * 1000);
    cur_hash->Reset();

    return 0;
}
```

### 策略应用

待实现。。。

## 7. DB连接管理
### DBPool
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

### DBManager
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

## 8. Redis连接管理
### RedisWrapper
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

### RedisManager
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

## 9. 更多功能
更新中。。。
