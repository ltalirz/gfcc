# if( referencelapack_PREFERS_STATIC )
#   set( referencelapack_LIBRARY_NAME "liblapack.a" )
# else()
set( referencelapack_LIBRARY_NAME liblapack.a lapack )

set( referencelapack_SPREFIX "${STAGE_INSTALL_DIR}" )
set( referencelapack_IPREFIX "${CMAKE_INSTALL_PREFIX}" )

find_library( referencelapack_LIBRARY
  NAMES ${referencelapack_LIBRARY_NAME}
  HINTS ${referencelapack_SPREFIX} ${referencelapack_IPREFIX}
  PATHS ${referencelapack_LIBRARY_DIR} ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES} 
  PATH_SUFFIXES lib lib64 lib32
  DOC "ReferenceLAPACK Library"
)

find_package(StandardFortran REQUIRED)

#if( referencelapack_INCLUDE_DIR )
#  set( ReferenceLAPACK_INCLUDE_DIR ${referencelapack_INCLUDE_DIR} )
#endif()

if( referencelapack_LIBRARY AND STANDARDFORTRAN_LIBRARIES )
  set( ReferenceLAPACK_LIBRARIES ${referencelapack_LIBRARY} ${STANDARDFORTRAN_LIBRARIES} )
endif()


# Reference LAPACK is always LP64
set( ReferenceLAPACK_ilp64_FOUND FALSE )
set( ReferenceLAPACK_lp64_FOUND  TRUE  )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args( ReferenceLAPACK
#  REQUIRED_VARS ReferenceLAPACK_LIBRARIES ReferenceLAPACK_INCLUDE_DIR
  REQUIRED_VARS ReferenceLAPACK_LIBRARIES
#  VERSION_VAR ReferenceLAPACK_VERSION_STRING
  HANDLE_COMPONENTS
)

if( ReferenceLAPACK_FOUND AND NOT TARGET ReferenceLAPACK::lapack )

  add_library( ReferenceLAPACK::lapack INTERFACE IMPORTED )
  set_target_properties( ReferenceLAPACK::lapack PROPERTIES
#    INTERFACE_INCLUDE_DIRECTORIES "${ReferenceLAPACK_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES      "${ReferenceLAPACK_LIBRARIES}"
  )

endif()

