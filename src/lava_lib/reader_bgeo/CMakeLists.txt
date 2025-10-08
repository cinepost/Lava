file( GLOB SOURCES
    ./bgeo/*.cpp
    ./bgeo/parser/*.cpp
)

include_directories(
    ${HOUDINI_ROOT}/toolkit/include
)

add_library( reader_bgeo_lib SHARED ${SOURCES} ${HEADERS} )

if(WIN32)
    set( 
        HOU_LIBS 
        ${HOUDINI_ROOT}/custom/houdini/dsolib/libUT.lib
        ${HOUDINI_ROOT}/custom/houdini/dsolib/libSYS.lib
    )
    add_definitions(-DAMD64 -DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P})
else()
    set( 
        HOU_LIBS 
        ${HOUDINI_ROOT}/dsolib/libHoudiniUT.so
    )
    add_definitions(-DAMD64 -DLINUX -DUSE_PTHREADS -DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P})
endif()

if(WIN32)
    set_target_properties(reader_bgeo_lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    add_compile_definitions($<$<CONFIG:DEBUG>:__TBB_LIB_NAME=tbb.lib>)
endif()

target_link_libraries(
    reader_bgeo_lib
    lava_utils_lib
    ${HOU_LIBS}
)

if(WIN32)
    target_link_libraries( 
        reader_bgeo_lib
        ${HOUDINI_ROOT}/custom/houdini/dsolib/tbb.lib
    )
endif()

if(UNIX)
    install(
        TARGETS reader_bgeo_lib
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
elseif(WIN32)
    install(
        TARGETS reader_bgeo_lib
        RUNTIME DESTINATION bin
    )
    install(
        FILES 
            ${HOUDINI_ROOT}/bin/libUT.dll
            ${HOUDINI_ROOT}/bin/libSYS.dll
        DESTINATION bin
    )
endif()
