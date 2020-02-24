cmake_minimum_required(VERSION 3.0)

##############################
# Compiler flags
##############################

set(WARNING_FLAGS "${WARNING_FLAGS} -Werror -Wno-unused-function -Wno-unused-variable -Wno-shorten-64-to-32")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
if (IOS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DIOS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DIOS")
endif()
if (WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWINDOWS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWINDOWS")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 ${WARNING_FLAGS} -fPIC -g2 -DLOG_USE_COLOR")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS} -fPIC  -DLOG_USE_COLOR")
endif()

##############################
# Macros
##############################
macro(pre_build)
    set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX})
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
    
    # Sources
    file(GLOB_RECURSE CUR_PROJ_HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.h" "*.hpp")   
    aux_source_directory(. CUR_PROJ_SOURCES)    
endmacro()

macro(post_build)    
    # Includes
    separate_arguments(PROJ_INCLUDE_DIR)
    target_include_directories(${CUR_PROJ} PUBLIC ${PROJ_INCLUDE_DIR})

    foreach(depend_lib ${DEPENDS_LIBS})
        set(PROJ_INCLUDE_DIR "${PROJ_INCLUDE_DIR} ${PROJ_ROOT}/${depend_lib}")
        string(STRIP ${depend_lib} depend_lib)
        add_dependencies(${CUR_PROJ} ${depend_lib})
        target_link_libraries(${CUR_PROJ} ${depend_lib})    
    endforeach()

    foreach(depend_lib ${EXTERNAL_LIBS})
        string(STRIP ${depend_lib} depend_lib)
        target_link_libraries(${CUR_PROJ} ${depend_lib})    
    endforeach()

    # Deploy
    deploy_build()
endmacro()

macro(file_name_macro_set)
    foreach(f ${CUR_PROJ_SOURCES})
        get_filename_component(b ${f} NAME)
        set_source_files_properties(${f} PROPERTIES
        COMPILE_DEFINITIONS "__FILE_NAME__=\"${b}\"")
    endforeach()    
endmacro(file_name_macro_set)


macro(build_library LIB_TYPE)
    pre_build()
    file_name_macro_set()
    add_library(${CUR_PROJ} ${LIB_TYPE} ${CUR_PROJ_HEADERS} ${CUR_PROJ_SOURCES}) 

    post_build()
endmacro()

macro(build_executable)
    pre_build()
    file_name_macro_set()

    add_executable(${CUR_PROJ} ${CUR_PROJ_HEADERS} ${CUR_PROJ_SOURCES})
    
    if (WIN32)
        set_property(TARGET ${CUR_PROJ} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "../mlc/${CMAKE_BUILD_TYPE}")
    endif()

    post_build()
endmacro()

macro(deploy_build)
    install(TARGETS ${CUR_PROJ} 
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}
        BUNDLE DESTINATION ${CMAKE_INSTALL_PREFIX})
endmacro()


macro(declare_current_proj)
    get_filename_component(CUR_PROJ ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    project(${CUR_PROJ})
    set(PROJ_INCLUDE_DIR ${PROJ_ROOT})
    set(PROJ_INCLUDE_DIR "${PROJ_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}")
endmacro()

macro(depens_on)
    set(DEPENDS_LIBS "${DEPENDS_LIBS} ${ARGN}")
endmacro()

macro(add_external_lib)
    set(EXTERNAL_LIBS "${EXTERNAL_LIBS} ${ARGN}")
endmacro()
