cmake_minimum_required(VERSION 3.0)

include_directories(${CMAKE_CURRENT_LIST_DIR})
set(BASE_LIB_PATH ${CMAKE_CURRENT_LIST_DIR}/base_libs)

function(add_custom_lib_source SRCS)
    set(options ALL COMMON_UTIL STRING_UTIL VALIDATOR)
    cmake_parse_arguments(common_src "${options}" "" "" "" ${ARGN})

    if (common_src_COMMON_UTIL OR common_src_ALL)
        aux_source_directory(${BASE_LIB_PATH}/common_utils _src)
        message("[+] add source:" ${BASE_LIB_PATH}/common_utils)
        list(APPEND SRCS ${_src})
    endif()

    if (common_src_STRING_UTIL OR common_src_ALL)
        aux_source_directory(${BASE_LIB_PATH}/string_utils _src)
        message("[+] add source:" ${BASE_LIB_PATH}/string_utils)
        list(APPEND SRCS ${_src})
    endif()

    if (common_src_VALIDATOR OR common_src_ALL)
        aux_source_directory(${BASE_LIB_PATH}/validator _src)
        message("[+] add source:" ${BASE_LIB_PATH}/validator)
        list(APPEND SRCS ${_src})
    endif()

    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
endfunction()