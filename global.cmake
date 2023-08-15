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
    aux_source_directory(${ROOT_PATH}/core/server _src)
    aux_source_directory(${ROOT_PATH}/core/config _src)
    aux_source_directory(${ROOT_PATH}/core/log _src)
    message("[+] add server source:" ${ROOT_PATH}/core)
    list(APPEND SRCS ${_src} ${SVR_PB_SRCS})
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
endfunction()

set(BASE_LIB_PATH ${ROOT_PATH}/base_libs)

function(add_custom_lib_source SRCS)
    set(options ALL COMMON_UTIL STRING_UTIL VALIDATOR)
    cmake_parse_arguments(common_src "${options}" "" "" "" ${ARGN})

    if (common_src_COMMON_UTIL OR common_src_ALL)
        aux_source_directory(${BASE_LIB_PATH}/common_utils _src)
        message("[+] add custom source:" ${BASE_LIB_PATH}/common_utils)
        list(APPEND SRCS ${_src})
    endif()

    if (common_src_STRING_UTIL OR common_src_ALL)
        aux_source_directory(${BASE_LIB_PATH}/string_utils _src)
        message("[+] add custom source:" ${BASE_LIB_PATH}/string_utils)
        list(APPEND SRCS ${_src})
    endif()

    if (common_src_VALIDATOR OR common_src_ALL)
        aux_source_directory(${BASE_LIB_PATH}/validator _src)
        message("[+] add custom source:" ${BASE_LIB_PATH}/validator)
        list(APPEND SRCS ${_src})
    endif()

    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
endfunction()