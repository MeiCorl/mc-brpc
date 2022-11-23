cmake_minimum_required(VERSION 3.0)

project(proto_test)

set(CMAKE_CXX_STANDARD 11)

set(BIN_NAME "proto_test")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../brpc.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../global.cmake)

EXECUTE_PROCESS(
    COMMAND rm ${CMAKE_CURRENT_SOURCE_DIR}/proto/validator.proto
    COMMAND ln -s ${CMAKE_CURRENT_SOURCE_DIR}/../../base_libs/validator/proto/validator.proto ${CMAKE_CURRENT_SOURCE_DIR}/proto/validator.proto
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 编译proto文件
# find_package(Protobuf REQUIRED)
file(GLOB PROTOS
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto"
)
protobuf_generate_cpp(PB_SRCS PB_HDRS ${PROTOS})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRCS)
message("[+] add source:" ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_custom_lib_source(COMM_SRCS VALIDATOR)

# 生成目标文件
add_library(common_objs OBJECT ${SRCS} ${COMM_SRCS} ${PB_SRCS})

# 生成可执行文件
add_executable(${BIN_NAME} "main.cpp" $<TARGET_OBJECTS:common_objs>)

target_link_libraries(${BIN_NAME} ${BRPC_LIB} ${DYNAMIC_LIB})