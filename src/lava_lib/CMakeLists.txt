file( GLOB SOURCES
    ./*.cpp
)

add_subdirectory(reader_bgeo)
add_subdirectory(reader_lsd)
add_subdirectory(houdini_display)
add_subdirectory(sdl_display)

# Python
find_package( PythonLibs 3.7 REQUIRED )
include_directories( ${PYTHON_INCLUDE_DIRS} )

# FreeImage
#find_package(FreeImage REQUIRED)
#include_directories( ${FREEIMAGE_INCLUDE_DIRS} )

# Find Boost libs
find_package( Boost COMPONENTS system filesystem REQUIRED )
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS}) 
endif()

include_directories( ${Boost_INCLUDE_DIRS} )

# We use Boost static libs
add_definitions( -DBOOST_LOG_DYN_LINK )


add_library( lava_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    lava_lib
    reader_lsd_lib
    falcor_lib
    lava_utils_lib
    ${Boost_LIBRARIES}

    TexturesResolvePass
    AccumulatePass
    DepthPass
    ForwardLightingPass
    SkyBox
    HBAO
    MinimalPathTracer
    GBuffer
)

if(UNIX)
    install(TARGETS lava_lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    )
endif()
