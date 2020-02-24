cmake_minimum_required(VERSION 2.8)
include(${PROJ_ROOT}/config.cmake)

if(APPLE)
    include_directories(SYSTEM 
        /System/Library/Frameworks/Python.framework/Versions/2.7/include/python2.7)
elseif(UNIX)
    include_directories(SYSTEM 
        /usr/include/python2.7)
endif()

declare_current_proj()
depens_on(mlc)
build_library("SHARED")

SET_TARGET_PROPERTIES(${CUR_PROJ} PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(${CUR_PROJ} PROPERTIES OUTPUT_NAME "mlc_py")