cmake_minimum_required(VERSION 2.8...3.27)

project(brpc_test)

set(BIN_NAME "brpc_test_d")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../global.cmake)

EXECUTE_PROCESS(
    COMMAND rm ${CMAKE_CURRENT_SOURCE_DIR}/proto/validator.proto
    COMMAND ln -s ${CMAKE_CURRENT_SOURCE_DIR}/../../core/utils/validator/proto/validator.proto ${CMAKE_CURRENT_SOURCE_DIR}/proto/validator.proto
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# add_definitions(-DLOCAL_TEST)
add_definitions(-DUSE_ASYNC_LOGSINK)

# 编译proto文件
# find_package(Protobuf REQUIRED)
file(GLOB PROTOS
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto"
)
protobuf_generate_cpp(PB_SRCS PB_HDRS ${PROTOS})

file(GLOB TEST_PROTO "${CMAKE_CURRENT_SOURCE_DIR}/proto/test.proto")
auto_gen_client_code(${CMAKE_CURRENT_SOURCE_DIR}/client ${CMAKE_CURRENT_SOURCE_DIR}/proto/ ${TEST_PROTO})

add_server_source(SERVER_SRCS)
add_custom_lib_source(COMM_SRCS VALIDATOR)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRCS)
message("[+] add source:" ${CMAKE_CURRENT_SOURCE_DIR}/src)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/client SRCS)
message("[+] add client source:" ${CMAKE_CURRENT_SOURCE_DIR}/client)

# 生成目标文件
add_library(common_objs OBJECT ${SERVER_SRCS} ${SRCS} ${COMM_SRCS} ${PB_SRCS})

# 生成可执行文件
add_executable(${BIN_NAME} "main.cpp" $<TARGET_OBJECTS:common_objs>)

target_link_libraries(${BIN_NAME} ${BRPC_LIB} ${DYNAMIC_LIB} etcd-cpp-api)