# mc-brpc
### &emsp;&emsp; mc-brpc是基于[百度brpc框架](https://brpc.apache.org/)快速开发rpc服务的集成框架，目的是简化构建brpc服务以及发起brpc请求的流程和代码开发量。主要在brpc的基础上增加了以下功能：
* **服务注册**：brpc对于rpc服务中一些功能并未提供具体实现，而是需要用户自己去扩展实现。如服务注册，因为不同用户使用的服务注册中心不一样，有Zookeeper、Eureka、Nacos、Consul、Etcd、甚至自研的注册中心等，不同注册中心注册和发现流程可能不一样，不太好统一，因此只好交由用户根据去实现服务注册这部分；而对于服务发现这部分，brpc默认支持基于文件、dns、bns、http、consul、nacos等的服务发现，并支持用户扩展NamingService实现自定义的服务发现(McNamingService)。mc-brpc在brpc的基础上实现了服务自动注册到etcd，并扩展了NamingService实现了自定义的服务发现(mc:\\\\service_name)

* **名字服务代理**：mc-brpc服务启动会自动注册到etcd，但却不直接从etcd直接做服务发现，而是提供了一个brpc_name_agent基础服务作为名字服务代理，它负责从etcd实时更新服务信息，并为mc-brpc提供服务发现，主要是为了支持在服务跨机房甚至跨大区部署时，brpc请求能支持按指定大区和机房进行路由，此外name_agent还将服务信息dump出来作为promethus监控的targets，以及后续拟支持户端主动容灾功(暂未实现)等

* **日志异步刷盘**：brpc提供了日志刷盘抽象工具类LogSink，并提供了一个默认的实现DefaultLogSink，但是DefaultLogSink写日志是同步写，且每写一条日志都会写磁盘，性能较差，在日志量大以及对性能要求较高的场景下很难使用，而百度内部使用的ComlogSink实现似乎未开源(看代码没找到)，因此mc-brpc自己实现了个AsyncLogSink先将日志写缓冲区再批量刷盘(也得感谢brpc插件式的设计，极大的方便了用户自己做功能扩展)，AsyncLogSink写日志先写缓冲区，再由后台线程每秒批量刷盘，性能相比DefaultLogSink由10倍以上提升，但是在服务崩溃的情况下可能会丢失最近1s内的日志

* **日志自动归档**：LogRotateWatcher及LogArchiveWorker每小时对日志进行归档压缩，方便日志查询，并删除一个月以前的日志(时间可以通过配置指定)避免磁盘写满

* **自动生成rpc客户端代码**：当通过某个服务通过proto文件定义好接口后，其它服务若想调用该服务的接口，只需要在CMakeLists.txt(参考services/brpc_test/CmakeLists.txt)中调用auto_gen_client_code即可为指定proto生成对应的同步客户端(SyncClient)、半同步客户端(SemiSyncClient)及异步客户端(ASyncClient)，简化发起brpc调用的流程。其原理是通过一个mc-brpc提供的protobuf插件codexx(core/plugins/codexx)解析对应proto文件然后生成相应客户端代码

* **统一全局配置**：mc-brpc服务启动会自动解析gflags(如果有../conf/gflags.conf配置文件)；并自动解析服务配置(../conf/server.conf)，其中包含服务名、名字服务地址、大区id、机房id、DB配置、Redis配置、日志相关配置等，并自动根据服务配置对相关组件进行初始化操作(如：根据DB配置创建DB连接池，根据Redis配置初始化Redis客户端代理实例等)

* **DB连接管理**：core/mysql下基于[libmysqlclient](https://dev.mysql.com/downloads/c-api/)封装实现了MysqlConn并提供了对应连接池实现DBPool，mc-brpc服务启动时会根据配置为每个DB实例初始化一个连接池对象并注册到一个全局map中，使用时通过DBManager从对应实例的连接池获取一个连接进行DB操作。DBPool支持设置连接池最小空闲连接数、最大活跃连接数、获取链接超时时间、连接空闲超时时间（长期空闲超时的连接会被自动释放）等

* **Redis连接管理**：core/redis下基于[redis-plus-plus](https://github.com/sewenew/redis-plus-plus)封装实现了客户端代理类RedisWrapper(主要目的在于对业务代码屏蔽redis++在操作单实例redis、哨兵模式redis以及集群模式redis的差异)。同DB连接管理一样，mc-brpc服务启动时会根据配置为每个Redis(单实例/Sentine/Cluster)初始化一个RedisWrapper对象并注册到一个全局map中，使用时通过RedisManager从对应redis(集群)获取一个连接进行操作。这里redis连接池的管理就不需要做额外实现了，redis++会为每个redis实例创建一个连接池并管理连接(如果是集群则为每个master节点创建一个连接池)

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
│       ├── brpc_name_agent    &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp; # 名字服务代理(作为一个基础服务，需要打包部署到每个容器镜像中，为该容器内的其它服务提供服务发现)  
│       ├── brpc_test  &emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp; # 样例服务  