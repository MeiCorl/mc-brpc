cmake_minimum_required(VERSION 2.8...3.27)

project(brpc_name_agent)

set(CMAKE_CXX_STANDARD 11)

set(BIN_NAME "brpc_name_agent_d")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../global.cmake)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

file(GLOB PROTOS
    "${ROOT_PATH}/core/common/name_agent.proto"
)
protobuf_generate_cpp(PB_SRCS PB_HDRS ${PROTOS})

add_server_source(SERVER_SRCS)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRCS)
message("[+] add source:" ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 生成目标文件
add_library(common_objs OBJECT ${SERVER_SRCS} ${SRCS} ${COMM_SRCS} ${PB_SRCS})

# 生成可执行文件
add_executable(${BIN_NAME} "main.cpp" $<TARGET_OBJECTS:common_objs>)

target_link_libraries(${BIN_NAME} ${BRPC_LIB} ${DYNAMIC_LIB} etcd-cpp-api)