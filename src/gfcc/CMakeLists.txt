cmake_minimum_required(VERSION ${CMAKE_VERSION})
project(GFCC VERSION ${PROJECT_VERSION} LANGUAGES CXX)
include(TargetMacros) #For nwchemex_add_library

#Add the current directory's sources to the list
set(GFCC_SRCS gf_ccsd.cpp)

#Add the current directory's header files to the list
set(GFCC_INCLUDES
    gf_ccsd.hpp  gf_guess.hpp      gfccsd_ip.hpp
    contrib/ccsd_util.hpp          contrib/cd_svd_ga.hpp  
    contrib/cd_ccsd_os_ann.hpp     contrib/diis.hpp       
    contrib/scf_iter.hpp           contrib/scf_guess.hpp
    contrib/scf_main.hpp           contrib/scf_common.hpp
    contrib/scf_taskmap.hpp        contrib/molden.hpp
    contrib/json_data.hpp          contrib/misc.hpp  
    contrib/cd_ccsd_os_ann.hpp     contrib/two_index_transform.hpp
    contrib/input_parser.hpp       
   )

set(GFCC_CFLAGS )
set(GFCC_LFLAGS )

cmsb_add_library(gfcc GFCC_SRCS GFCC_INCLUDES GFCC_CFLAGS GFCC_LFLAGS)
target_include_directories(gfcc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_include_directories(gfcc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/external)


