#set(PXR_PREFIX pxr/imaging)
#set(PXR_PACKAGE hdLava)

# Python
find_package( PythonLibs 2.7 REQUIRED )
include_directories( ${PYTHON_INCLUDE_DIRS} )

add_custom_target(shared_libs)

pxr_plugin( hdLava
  LIBRARIES
    #ar
    #trace
    #plug
    #tf
    #gf
    #hf
    #ndr
    #usdVol
    #usdLux
    #usdUtils
    #usdRender
    #usdGeom
    #usdImaging
    #pxOsd
    #cameraUtil
    #falcor_lib
    lava_lib

  INCLUDE_DIRS
    ${Boost_INCLUDE_DIRS}
    ${TBB_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${OPENEXR_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/src/lava
    ${PROJECT_SOURCE_DIR}/src/pxr/imaging/plugin/hdLava
    /opt/houdini18.5/toolkit/include

  PRIVATE_CLASSES
    #instancer
    #mesh
    #material
    config
    camera
    rendererPlugin
    renderParam
    renderDelegate
    renderPass
    renderBuffer
    renderThread
    lavaApi
    #lavaApiAov
    #lavaApiFramebuffer
    #debugCodes
    #sampler
    #texture
    #lights/light
    #lights/diskLight
    #lights/distantLight
    #lights/domeLight
    #lights/rectLight
    #lights/sphereLight
    #lights/simpleLight

  PRIVATE_HEADERS
    boostIncludePath.h
    #error.h
    api.h

  #RESOURCE_FILES
  #  plugInfo.json
)

if ( _houdini_platform_linux )
    # Link against Houdini libraries (including USD)
    if ( DEFINED ENV{HOUDINI_HDK_LINK_GUSD} )
        target_link_libraries( hdLava
            # Standard Houdini librarys
            ${_houdini_hfs_root}/dsolib/libgusd.so
        )
    else ()
        target_link_libraries( hdLava
        Houdini
    )
    endif ()
endif ()

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
target_include_directories(hdLava PUBLIC ${PROJECT_SOURCE_DIR}/src/pxr/imaging/plugin/hdLava)

#GroupSources(hdLava)

install(
    CODE
    "FILE(WRITE \"${CMAKE_INSTALL_PREFIX}/houdini/dso/usd_plugins/plugInfo.json\"
    \"{
    \\\"Includes\\\": [ \\\"*/resources/\\\" ]
    }\")"
)

set(PLUG_INFO_LIBRARY_PATH "../../usd/hdLava.so")
set(PLUG_INFO_RESOURCE_PATH "resources")
set(PLUG_INFO_ROOT "..")
configure_file(plugInfo.json.in ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json.install @ONLY)

install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json.install
    RENAME
    plugInfo.json
    DESTINATION
    ${CMAKE_INSTALL_PREFIX}/houdini/dso/usd_plugins/hdLava/resources/
)

install( TARGETS hdLava DESTINATION "${CMAKE_INSTALL_PREFIX}/houdini/dso/usd/" )
