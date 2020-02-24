cmake_minimum_required(VERSION 3.0)

if (NOT IOS AND NOT ANDROID)
    set(BUILD_TEST TRUE)
endif()

if (UNIX AND (NOT ANDROID AND NOT APPLE))
    set(BUILD_PLUGIN FALSE)
endif()

set(PROJ_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(mlc)

if (BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()

if (BUILD_TEST)
    add_subdirectory(testserver)
    add_subdirectory(bbx_demo)
    add_subdirectory(testclient)
    add_subdirectory(testfec)
    add_subdirectory(testkcp)
    add_subdirectory(mlc_tun_client)
    add_subdirectory(mlc_tun_server)
    add_subdirectory(mlc_proxy)
endif()
