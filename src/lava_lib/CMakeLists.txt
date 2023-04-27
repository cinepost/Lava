include_directories( 
    ${EXTERNALS_INSTALL_DIR}
)

file( GLOB SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/../version.cpp
    ./display_oiio.cpp
    ./*.cpp
)

include_directories(
  ${HOUDINI_ROOT}/toolkit/include
)
# add_definitions(-DAMD64 -DLINUX -DUSE_PTHREADS -DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P})
add_definitions(-DAMD64 -DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P} -DLAVA_DLL)

add_subdirectory(reader_bgeo)
add_subdirectory(reader_lsd)
add_subdirectory(houdini_display)
add_subdirectory(sdl_display)

include_directories( ${Boost_INCLUDE_DIRS} )

# Use FindVulkan module added with CMAKE 3.7
if (NOT CMAKE_VERSION VERSION_LESS 3.7.0)
    message(STATUS "Using module to find Vulkan")
    find_package(Vulkan REQUIRED)
endif()

include_directories( ${Vulkan_INCLUDE_DIRS} )

set(dlfcn-win32_DIR "${EXTERNALS_INSTALL_DIR}/dlfcn-win32/share/dlfcn-win32")
find_package(dlfcn-win32 REQUIRED)
# include_directories(${dlfcn-win32_INCLUDE_DIRS})

add_definitions(-DBOOST_ALL_DYN_LINK)

add_compile_definitions(
    $<$<CONFIG:DEBUG>:__TBB_LIB_NAME=tbb.lib>
)

add_library( lava_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    lava_lib
    reader_lsd_lib
    falcor_lib
    lava_utils_lib
    TexturesResolvePass
    AccumulatePass
    AmbientOcclusionPass
    EdgeDetectPass
    DepthPass
    DebugShadingPass
    DeferredLightingPass
    PathTracer
    CryptomattePass
    ToneMapperPass
    OpenDenoisePass
    ImageLoaderPass
    SkyBox
    RTXDIPass
    GBuffer
    # ${dlfcn-win32_LIBRARIES}
    dlfcn-win32::dl
    # ${HOUDINI_ROOT}/custom/houdini/dsolib/OpenImageIO_sidefx.lib
    # ${HOUDINI_ROOT}/custom/houdini/dsolib/OpenImageIO_Util_sidefx.lib
)

if(UNIX OR WIN32)
    install(TARGETS lava_lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    )
endif()
