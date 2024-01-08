# 简介
### &emsp;&emsp; mc-brpc是基于[百度brpc框架](https://brpc.apache.org/)快速开发rpc服务的集成框架，目的是简化构建brpc服务以及发起brpc请求的流程和代码开发量。主要在brpc的基础上增加了以下功能：
* **服务注册**：brpc对于rpc服务中一些功能并未提供具体实现，而是需要用户自己去扩展实现。如服务注册，因为不同用户使用的服务注册中心不一样，有Zookeeper、Eureka、Nacos、Consul、Etcd、甚至自研的注册中心等，不同注册中心注册和发现流程可能不一样，不太好统一，因此只好交由用户根据去实现服务注册这部分；而对于服务发现这部分，brpc默认支持基于文件、dns、bns、http、consul、nacos等的服务发现，并支持用户扩展NamingService实现自定义的服务发现。mc-brpc在brpc的基础上实现了服务自动注册到etcd，并提供了NamingService的扩展(<font color=#00ffff>McNamingService</font>)用以支持自定义的服务发现(**mc:\\\\service_name**)

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
## MCServer
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
1. <font color=#00ffff>MCServer</font>包含了brpc::Server，因此MCServer实例化首先也会触发brpc::Server的实例化。  
2. 判断是否有gflags.conf配置文件(../conf/gflags.conf)，如果有则自动触发gflags解析。可以在这里通过gflags配置控制一些brpc功能的开关(如优雅退出、rpcz等，参考services/brpc_test/conf/gflags.conf)，以及设置自定义gflags参数。
3. 解析服务配置文件(../conf/server.conf)，里面包含注册中心地址、服务名、所在大区id、机房id、DB配置、Redis配置、日志配置等(参考services/brpc_test/conf/server.conf)。
4. 根据3中服务日志配置，初始化日志输出文件路径并创建日志文件监听线程和日志归档线程(二者配合实现日志分时归档压缩)，以及判断是否需要使用异步日志写入AsyncLogSink(需要CMakeLists.txt中定义<font color=#ffff00>USE_ASYNC_LOGSINK</font>宏)。
5. 根据3中服务配置，初始化DB连接池(需要CMakeLists.txt中定义<font color=#ffff00>USE_MYSQL</font>宏)及Redis连接池(需要CMakeLists.txt中定义<font color=#ffff00>USE_REDIS</font>宏)。
6. 注册自定义的名字服务<font color=#00ffff>McNamingService</font>用以支持**mc:\\\\** 前缀的服务发现

### 启动过程
1. 首先指定服务监听地址。可以通过gflags<*listen_addr*>指定ip:port或者unix_socket地址, 否则使用当前ip地址+随机端口。调brpc::Start启动server
2. 将服务信息(ip:port、服务名、大区id、机房id)注册到服务中心。这里使用etcd作为服务中心，并通过etcd的租约续期功能为注册信息续期。
3. 调brpc::RunUntilAskedToQuit死循环直到进程被要求退出。

### 退出过程
退出过程相对简单。销毁日志监听线程、日志归档线程，从服务中心取消注册并停止etcd租约续期等。