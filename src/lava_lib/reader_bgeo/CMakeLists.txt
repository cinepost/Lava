file( GLOB SOURCES
    ./bgeo/*.cpp
    ./bgeo/parser/*.cpp
)

include_directories(
  ${HOUDINI_ROOT}/toolkit/include
)

# set( HOU_LIBS ${HOUDINI_ROOT}/dsolib/libHoudiniUT.so)
set( HOU_LIBS ${HOUDINI_ROOT}/custom/houdini/dsolib/libUT.lib)

# add_definitions(-DAMD64 -DLINUX -DUSE_PTHREADS -DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P})
add_definitions(-DAMD64 -DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P})
add_library( reader_bgeo_lib SHARED ${SOURCES} ${HEADERS} )

set_target_properties(reader_bgeo_lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

add_compile_definitions(
  $<$<CONFIG:DEBUG>:__TBB_LIB_NAME=tbb.lib>
)

target_link_libraries(
    reader_bgeo_lib
    ${HOU_LIBS}
    ${HOUDINI_ROOT}/custom/houdini/dsolib/tbb.lib
    ${HOUDINI_ROOT}/custom/houdini/dsolib/libSYS.lib
    # ${HOUDINI_ROOT}/python37/libs/python37.lib
)

if(UNIX OR WIN32)
    install(TARGETS reader_bgeo_lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    )
endif()
