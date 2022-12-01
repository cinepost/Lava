include_directories( 
    ${EXTERNALS_INSTALL_DIR}
)

file( GLOB SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/../version.cpp
    ./display_oiio.cpp
    ./*.cpp
)

add_subdirectory(reader_bgeo)
add_subdirectory(reader_lsd)
add_subdirectory(houdini_display)
add_subdirectory(sdl_display)

include_directories( ${Boost_INCLUDE_DIRS} )

add_library( lava_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    lava_lib
    reader_lsd_lib
    falcor_lib
    lava_utils_lib
    TexturesResolvePass
    AccumulatePass
    DepthPass
    ForwardLightingPass
    DeferredLightingPass
    DeferredLightingCachedPass
    PathTracer
    CryptomattePass
    ToneMapperPass
    OpenDenoisePass
    ImageLoaderPass
    SkyBox
    RTXDIPass
    GBuffer
)

if(UNIX)
    install(TARGETS lava_lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    )
endif()
