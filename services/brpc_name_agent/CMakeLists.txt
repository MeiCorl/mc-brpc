cmake_minimum_required(VERSION 2.8...3.27)

project(brpc_name_agent)

set(BIN_NAME "brpc_name_agent_d")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../global.cmake)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# file(GLOB PROTOS
#     "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto"
# )
# protobuf_generate_cpp(PB_SRCS PB_HDRS ${PROTOS})

# 自定义name_agent.proto代码生成路径到core/extensions供其他服务使用
set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/name_agent.proto)
set(PROTO_PATH ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(OUT_PATH ${ROOT_PATH}/core/extensions)
execute_process(
    COMMAND /usr/bin/protoc --proto_path=${PROTO_PATH} --cpp_out=${OUT_PATH} ${PROTO_FILE}
)

add_server_source(SERVER_SRCS)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRCS)
message("[+] add source:" ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 生成目标文件
add_library(common_objs OBJECT ${SERVER_SRCS} ${SRCS} ${COMM_SRCS} ${PB_SRCS})

# 生成可执行文件
add_executable(${BIN_NAME} "main.cpp" $<TARGET_OBJECTS:common_objs>)
custom_link_libraries(${BIN_NAME})