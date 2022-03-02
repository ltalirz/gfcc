
cmake_minimum_required(VERSION 3.18.0)

project(GFCC VERSION 1.0.0 LANGUAGES C CXX Fortran)
include(${PROJECT_SOURCE_DIR}/cmake/check_user_env.cmake)

find_package( CMakeBuild QUIET )

if( NOT ${CMakeBuild_FOUND} )
  if (NOT CMSB_TAG)
    set(CMSB_TAG c7a01660058e096795baddba6158b9cdc51ca93e)
    if (ENABLE_DEV_MODE)
      set(CMSB_TAG master)
    endif()
  endif()

  FetchContent_Declare(
    CMakeBuild
    GIT_REPOSITORY https://github.com/NWChemEx-Project/CMakeBuild.git
    GIT_TAG ${CMSB_TAG}
  )

  FetchContent_MakeAvailable( CMakeBuild )
  set(CMSB_MACROS ${CMakeBuild_SOURCE_DIR}/cmake/macros)

  FetchContent_MakeAvailable( linalg-cmake-modules )
  set(CMSB_LAM_PATH ${linalg-cmake-modules_SOURCE_DIR})
endif()


set(CMSB_PROJECTS GFCC)

list(APPEND GFCC_DEPENDENCIES TAMM)

set(BUILD_METHODS OFF)

set(GFCC_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/gfcc)
set(GFCC_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/driver/)

if(NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif()

build_cmsb_module(${CMAKE_CURRENT_LIST_DIR})
