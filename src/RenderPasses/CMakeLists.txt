set( PASS_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/passes )

message("Render passes build dir: ${PASS_LIBRARY_OUTPUT_DIRECTORY}")

add_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")

include_directories( 
    ${PROJECT_SOURCE_DIR}/deps
    ${PROJECT_SOURCE_DIR}/deps/include/VulkanMemoryAllocator
    ${PROJECT_SOURCE_DIR}/deps/nvp
)
# external passes (can also be made built-in)
add_definitions(-DBUILD_DEPTH_PASS=TRUE)
add_definitions(-DBUILD_GAUSSIAN_PASS=TRUE)

# Python
find_package(PythonLibs 3.7 REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

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
    	${PYTHON_LIBRARIES}
    )

    # Copy/install all needed shaders
    set( SHADERS_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders/RenderPasses)
    file(MAKE_DIRECTORY ${SHADERS_OUTPUT_DIRECTORY})
    

    #if (WIN32)
    #file(
    #    COPY ${ARGS_NAME}
    #    DESTINATION ${SHADERS_OUTPUT_DIRECTORY} 
    #    FILES_MATCHING
    #        PATTERN "*.slang*"
    #)
    #else()
    #add_custom_command(
    #    TARGET ${PASS_LIB} 
    #    #POST_BUILD
    #    PRE_LINK
    #    COMMAND rsync
    #    ARGS -r --include='*.slang' --include='*/' --exclude='*'
    #        ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_NAME}
    #        ${SHADERS_OUTPUT_DIRECTORY}
    #    COMMENT "Copying ${ARGS_NAME} shaders"
    #)
    #endif (WIN32)

    # Install render-pass library
    
    install( 
        TARGETS ${PASS_LIB} 
        DESTINATION "render_passes" 
    )

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
makeRenderPassLib(NAME "Utils")
makeRenderPassLib(NAME "DepthPass")
makeRenderPassLib(NAME "BlitPass")
makeRenderPassLib(NAME "TexturesResolvePass")
makeRenderPassLib(NAME "HBAO")
makeRenderPassLib(NAME "ForwardLightingPass")
makeRenderPassLib(NAME "SkyBox")

makeRenderPassLib(NAME "WhittedRayTracer")
makeRenderPassLib(NAME "MinimalPathTracer")
makeRenderPassLib(NAME "MegakernelPathTracer")

#makeRenderPassLib(NAME "ToneMapper")
#makeRenderPassLib(NAME "ImageLoader")

#makeRenderPassLib(NAME "PixelInspectorPass")
makeRenderPassLib(NAME "AccumulatePass")
#makeRenderPassLib(NAME "Antialiasing")
#makeRenderPassLib(NAME "ErrorMeasurePass")
#makeRenderPassLib(NAME "DebugPasses")
#makeRenderPassLib(NAME "BSDFViewer")
#makeRenderPassLib(NAME "SVGFPass")

# Render passes with dependecies
makeRenderPassLib(NAME "GBuffer" DEPS DepthPass TexturesResolvePass)
#makeRenderPassLib(NAME "SSAO" DEPS Utils)
makeRenderPassLib(NAME "CSM" DEPS Utils)