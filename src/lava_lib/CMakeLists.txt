include_directories( 
    ${EXTERNALS_INSTALL_DIR}
)

file( GLOB SOURCES
    ./*.cpp
)

add_subdirectory(reader_bgeo)
add_subdirectory(reader_lsd)
add_subdirectory(houdini_display)
add_subdirectory(sdl_display)

# Find Boost libs
#find_package( Boost 1.73.0 COMPONENTS system filesystem REQUIRED )
#if(Boost_FOUND)
#    include_directories(${Boost_INCLUDE_DIRS}) 
#endif()

include_directories( ${Boost_INCLUDE_DIRS} )

# We use Boost static libs
#add_definitions( -DBOOST_LOG_DYN_LINK )


add_library( lava_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    lava_lib
    reader_lsd_lib
    falcor_lib
    lava_utils_lib
#    Boost::system
#    Boost::filesystem
    TexturesResolvePass
    AccumulatePass
    DepthPass
    ForwardLightingPass
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
