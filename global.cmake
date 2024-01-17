cmake_minimum_required(VERSION 2.8...3.27)

set(ROOT_PATH ${CMAKE_CURRENT_LIST_DIR})
include_directories(${ROOT_PATH})
message("[+] ROOT_PATH:" ${ROOT_PATH})

include(${ROOT_PATH}/brpc.cmake)

function(add_server_source SRCS)
    # file(GLOB SVR_PROTOS
    #     "${ROOT_PATH}/core/config/proto/*.proto"
    # )
    # protobuf_generate_cpp(SVR_PB_SRCS SVR_PB_HDRS ${SVR_PROTOS})
    # list(APPEND SRCS ${SVR_PB_SRCS})
    aux_source_directory(${ROOT_PATH}/core/server _src)
    aux_source_directory(${ROOT_PATH}/core/config _src)
    aux_source_directory(${ROOT_PATH}/core/log _src)
    aux_source_directory(${ROOT_PATH}/core/common _src)
    aux_source_directory(${ROOT_PATH}/core/extensions _src)
    message("[+] add server source:" ${ROOT_PATH}/server)
    message("[+] add server source:" ${ROOT_PATH}/config)
    message("[+] add server source:" ${ROOT_PATH}/log)
    message("[+] add server source:" ${ROOT_PATH}/common)
    message("[+] add server source:" ${ROOT_PATH}/extensions)

    set(options ALL ASYNCLOG MYSQL REDIS KAFKA)
    cmake_parse_arguments(custom_src "${options}" "" "" "" ${ARGN})

    if (custom_src_ASYNCLOG OR custom_src_ALL)
        add_definitions(-DUSE_ASYNC_LOGSINK)
        message("[+] using async log...")
    endif()

    if (custom_src_MYSQL OR custom_src_ALL)
        add_definitions(-DUSE_MYSQL)
        option (USE_MYSQL "Use ${ROOT_PATH}/core/mysql" ON)
        aux_source_directory(${ROOT_PATH}/core/mysql _src)
        message("[+] using mysql...")
    endif()

    if (custom_src_REDIS OR custom_src_ALL)
        add_definitions(-DUSE_REDIS)
        option (USE_REDIS "Use ${ROOT_PATH}/core/redis" ON)
        aux_source_directory(${ROOT_PATH}/core/redis _src)
        message("[+] using redis...")
    endif()

    list(APPEND SRCS ${_src})
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
endfunction()

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

function (custom_link_libraries bin_name)

    find_package(etcd-cpp-api REQUIRED)
    message("add library etcd-cpp-api...")
    target_link_libraries(${bin_name} ${BRPC_LIB} ${DYNAMIC_LIB} etcd-cpp-api)

    if(USE_MYSQL)
        message("add library mysqlclient...")
        target_link_libraries(${bin_name} mysqlclient)
    endif()

    if(USE_REDIS)
        find_library(HIREDIS_LIB hiredis)
        find_library(REDIS_PLUS_PLUS_LIB redis++)
        message("add library redis++...")
        target_link_libraries(${bin_name} ${HIREDIS_LIB} ${REDIS_PLUS_PLUS_LIB})
    endif()
endfunction()