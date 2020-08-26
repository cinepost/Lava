set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hdLava)
add_custom_target(shared_libs)

pxr_plugin(hdLava
  LIBRARIES
    ar
    trace
    plug
    tf
    gf
    hf
    ndr
    usdVol
    usdLux
    usdUtils
    usdRender
    usdGeom
    usdImaging
    pxOsd
    cameraUtil
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

  RESOURCE_FILES
    plugInfo.json
)

#find_package(ospray 2.2.0 REQUIRED)
#find_package(rkcommon 1.4.2 REQUIRED)

#target_link_libraries(hdLava
#  PUBLIC
#    falcor_lib
#)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
target_include_directories(hdLava PUBLIC ${PROJECT_SOURCE_DIR}/src/pxr/imaging/plugin/hdLava)

#GroupSources(hdLava)

install(
    CODE
    "FILE(WRITE \"${CMAKE_INSTALL_PREFIX}/plugin/plugInfo.json\"
    \"{
    \\\"Includes\\\": [ \\\"usd/*/resources/\\\" ]
}\")")

#if(UNIX)
#    install( TARGETS hdLava DESTINATION "/opt/USD/plugin/usd" )
#endif()
