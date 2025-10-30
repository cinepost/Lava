if(NOT EXTERNALS_INSTALL_DIR)
    set(EXTERNALS_INSTALL_DIR ${PROJECT_SOURCE_DIR}/deps/${CMAKE_BUILD_TYPE})
endif()

set( PASS_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/passes )

message("Render passes build dir: ${PASS_LIBRARY_OUTPUT_DIRECTORY}")

add_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")

# Python
find_package( Python3 REQUIRED COMPONENTS Development )

include_directories( 
    ${EXTERNALS_INSTALL_DIR}/include
    ${EXTERNALS_INSTALL_DIR}/include/slang
    ${PROJECT_SOURCE_DIR}/external/include
    ${Python3_INCLUDE_DIRS}
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source
)

# external passes (can also be made built-in)
add_definitions(-DBUILD_DEPTH_PASS=TRUE)
add_definitions(-DBUILD_GAUSSIAN_PASS=TRUE)

if(WIN32)
    message(STATUS "Using module to find Vulkan")
    find_package(Vulkan REQUIRED)
    include_directories(${Vulkan_INCLUDE_DIRS})
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
    add_definitions(-DBOOST_ALL_DYN_LINK)
    add_definitions(-DPASS_DLL)
endif()

# This function builds render pass library
function (makeRenderPassLib)
    cmake_parse_arguments(
        ARGS # prefix of output variables
        "" # list of names of the boolean arguments (only defined ones will be true)
        "NAME" # list of names of mono-valued arguments
        "SRCS;DEPS" # list of names of multi-valued arguments (output variables are lists)
        ${ARGN} # arguments of the function to parse, here we take the all original ones
    )

    if(NOT ARGS_NAME)
        message(FATAL_ERROR "You must provide a render pass name")
    endif(NOT ARGS_NAME)

	file( GLOB_RECURSE SOURCES
		./${ARGS_NAME}/*.cpp
    )

    file( GLOB_RECURSE SHADERS
        ./${ARGS_NAME}/*.slan*
    )

    set( PASS_LIB ${ARGS_NAME})

    add_library( ${PASS_LIB} SHARED ${SOURCES} ${HEADERS} )

    set_target_properties( ${PASS_LIB} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${PASS_LIBRARY_OUTPUT_DIRECTORY}
    )

    #ifdef(UNIX)
    set_target_properties( ${PASS_LIB} PROPERTIES PREFIX "" )
    set_target_properties( ${PASS_LIB} PROPERTIES SUFFIX ".rpl" ) # .rpl stands for "render pass library"
    #endif

    link_directories(
        ${PASS_LIBRARY_OUTPUT_DIRECTORY}
    )

    target_link_libraries(
    	${PASS_LIB}
    	falcor_lib
        ${ARGS_DEPS}
        lava_utils_lib
        gfx_lib
    )

    # Copy/install all needed shaders
    set( SHADERS_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders/RenderPasses)
    file(MAKE_DIRECTORY ${SHADERS_OUTPUT_DIRECTORY})

    # Install render-pass library
    if(UNIX)
        install(
            TARGETS ${PASS_LIB}
            DESTINATION "render_passes"
        )
    elseif(WIN32)
        install(
            TARGETS ${PASS_LIB}
            RUNTIME DESTINATION "render_passes"
        )
    endif()

    # Install render-pass shaders
    install(
        DIRECTORY ${ARGS_NAME}
        DESTINATION shaders/RenderPasses
        FILES_MATCHING PATTERN "*.slan*"
    )

    # Install render-pass data
    set( PASS_DATA_DIR "${PROJECT_SOURCE_DIR}/src/RenderPasses/${ARGS_NAME}/data/" )
    IF( IS_DIRECTORY "${PASS_DATA_DIR}" )
        message("DATA DIR " ${PASS_DATA_DIR})
        INSTALL(
            DIRECTORY ${PASS_DATA_DIR}
            DESTINATION data/${ARGS_NAME}
        )
    ENDIF()


endfunction()

# Now make bundled render passes

makeRenderPassLib(NAME "AccumulatePass")
makeRenderPassLib(NAME "AmbientOcclusionPass")
makeRenderPassLib(NAME "DepthPass")
makeRenderPassLib(NAME "TexturesResolvePass")
makeRenderPassLib(NAME "DeferredLightingPass")
makeRenderPassLib(NAME "DebugShadingPass")
makeRenderPassLib(NAME "NullShadingPass")
makeRenderPassLib(NAME "ToneMapperPass")
makeRenderPassLib(NAME "OpenDenoisePass")
makeRenderPassLib(NAME "CryptomattePass")
makeRenderPassLib(NAME "EdgeDetectPass")
makeRenderPassLib(NAME "ImageLoaderPass")
makeRenderPassLib(NAME "EnvironmentPass")
makeRenderPassLib(NAME "GBuffer" DEPS DepthPass TexturesResolvePass)

# Disabled passes

#makeRenderPassLib(NAME "PathTracer")
#makeRenderPassLib(NAME "RTXDIPass")


#makeRenderPassLib(NAME "DeferredLightingCachedPass")
#makeRenderPassLib(NAME "WhittedRayTracer")
#makeRenderPassLib(NAME "MinimalPathTracer")
#makeRenderPassLib(NAME "MegakernelPathTracer")

#makeRenderPassLib(NAME "ImageLoader")
#makeRenderPassLib(NAME "SkyBox")

#makeRenderPassLib(NAME "PixelInspectorPass")
#makeRenderPassLib(NAME "Antialiasing")
#makeRenderPassLib(NAME "ErrorMeasurePass")
#makeRenderPassLib(NAME "DebugPasses")
#makeRenderPassLib(NAME "BSDFViewer")
#makeRenderPassLib(NAME "SVGFPass")
#makeRenderPassLib(NAME "HBAO")
#makeRenderPassLib(NAME "Utils")
#makeRenderPassLib(NAME "BlitPass")

# Render passes with dependecies
#makeRenderPassLib(NAME "SSAO" DEPS Utils)
#makeRenderPassLib(NAME "CSM" DEPS Utils)