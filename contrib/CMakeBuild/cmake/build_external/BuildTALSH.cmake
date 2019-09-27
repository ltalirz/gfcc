

ExternalProject_Add(TALSH_External
    GIT_REPOSITORY https://github.com/DmitryLyakh/TAL_SH.git
    UPDATE_DISCONNECTED 1
    CMAKE_ARGS ${DEPENDENCY_CMAKE_OPTIONS}
    -DTALSH_GPU=ON -DTALSH_GPU_ARCH=${NWX_GPU_ARCH}
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install DESTDIR=${STAGE_DIR}
    CMAKE_CACHE_ARGS ${CORE_CMAKE_LISTS}
                     ${CORE_CMAKE_STRINGS}
)

