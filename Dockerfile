FROM ubuntu:latest
MAINTAINER meicorl

# 安装常用工具
RUN apt-get update -y
RUN apt install -y vim wget git lsof curl net-tools

# 安装c++编译工具
RUN apt install -y libssl-dev build-essential

# 安装cmake
RUN wget https://cmake.org/files/v3.27/cmake-3.27.0-linux-x86_64.tar.gz \
	&& tar -zxvf cmake-3.27.0-linux-x86_64.tar.gz \
	&& ln -s /cmake-3.27.0-linux-x86_64/bin/* /usr/local/bin

# 安装、运行etcd
RUN wget https://github.com/etcd-io/etcd/releases/download/v3.5.0/etcd-v3.5.0-linux-amd64.tar.gz \
	&& tar -zxvf etcd-v3.5.0-linux-amd64.tar.gz \
	&& ln -s /etcd-v3.5.0-linux-amd64/etcd /usr/local/bin/etcd \
	&& ln -s /etcd-v3.5.0-linux-amd64/etcdctl /usr/local/bin/etcdctl \
	&& ln -s /etcd-v3.5.0-linux-amd64/etcdutl /usr/local/bin/etcdutl \
	&& etcd &

# 安装protobuf & gRPC
RUN apt install -y libboost-all-dev libgrpc-dev libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc 

# 安装cpprestsdk
RUN  git clone https://github.com/microsoft/cpprestsdk.git \
 	&& cd cpprestsdk \
 	&& mkdir build \
 	&& cd build \
 	&& cmake .. -DCPPREST_EXCLUDE_WEBSOCKETS=ON \
	&& make -j$(nproc) \
	&& make install

# 安装etcd-cpp-apiv3
RUN git clone https://github.com/etcd-cpp-apiv3/etcd-cpp-apiv3.git \
	&& cd etcd-cpp-apiv3 \
	&& mkdir build \
	&& cd build \
	&& cmake .. \
	&& make -j$(nproc) \ 
	&& make install

# 安装brpc及其依赖
RUN apt install -y libgflags-dev libprotoc-dev protobuf-compiler libleveldb-dev
RUN git clone https://github.com/apache/brpc.git \ 
	&& cd brpc \
	&& sh config_brpc.sh --headers=/usr/include --libs=/usr/lib \
	&& make -j8 