cmake_minimum_required(VERSION 2.8)
include(${PROJ_ROOT}/config.cmake)

declare_current_proj()

aux_source_directory(core CUR_PROJ_SOURCES)
aux_source_directory(io_multiplexing CUR_PROJ_SOURCES)

if (WIN32)
    add_external_lib(ws2_32)
endif()

if ((APPLE AND NOT IOS) OR ANDROID OR WIN32)
    build_library("SHARED")
else()
    build_library("STATIC")
endif()