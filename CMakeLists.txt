
cmake_minimum_required(VERSION 3.18.0)

project(GFCC VERSION 1.0.0 LANGUAGES C CXX Fortran)
include(${PROJECT_SOURCE_DIR}/cmake/check_user_env.cmake)

find_package( CMakeBuild )

set(CMSB_PROJECTS GFCC)

list(APPEND GFCC_DEPENDENCIES TAMM)

set(BUILD_METHODS OFF)

set(GFCC_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/gfcc)
set(GFCC_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/driver/)

if(NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif()

build_cmsb_module(${CMAKE_CURRENT_LIST_DIR})
