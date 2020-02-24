cmake_minimum_required(VERSION 2.8)
include(${PROJ_ROOT}/config.cmake)

declare_current_proj()
depens_on(mlc)
build_executable()