file( GLOB SOURCES
    ./bgeo/*.cpp
    ./bgeo/parser/*.cpp
)

include_directories(
  ${HOUDINI_HOME}/toolkit/include
)

set( HOU_LIBS ${HOUDINI_HOME}/dsolib/libHoudiniUT.so)

add_definitions(-DAMD64 -DLINUX -DUSE_PTHREADS -DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P})
add_library( reader_bgeo_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    reader_bgeo_lib
    ${HOU_LIBS}
)

if(UNIX)
    install(TARGETS reader_bgeo_lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    )
endif()
