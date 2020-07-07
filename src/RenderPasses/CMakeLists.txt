set( PASS_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/passes )

add_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")

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

    set( PASS_LIB ${ARGS_NAME}_pass)

    add_library( ${PASS_LIB} SHARED ${SOURCES} ${HEADERS} )

    set_target_properties( ${PASS_LIB} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${PASS_LIBRARY_OUTPUT_DIRECTORY}
    )

    #ifdef(UNIX)
    set_target_properties( ${PASS_LIB} PROPERTIES PREFIX "" )
    set_target_properties( ${PASS_LIB} PROPERTIES SUFFIX ".dso" )
    #endif

    link_directories(
        /home/max/dev/Falcor/build/lib/passes/
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
    file(
        COPY ${pass_dir}
        DESTINATION ${SHADERS_OUTPUT_DIRECTORY} 
        FILES_MATCHING
            PATTERN "*.slang*"
    )

    # Copy comliped render-pass library
    set ( PASS_LIBRARY_INSTALL_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Passes")
    file(MAKE_DIRECTORY ${PASS_LIBRARY_INSTALL_DIRECTORY})
    add_custom_command(
        TARGET ${PASS_LIB} 
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${PASS_LIB}> ${PASS_LIBRARY_INSTALL_DIRECTORY}
    )

endfunction()

# Now make passes
makeREnderPassLib(NAME "Utils")
makeRenderPassLib(NAME "DepthPass")
makeRenderPassLib(NAME "BlitPass")
makeRenderPassLib(NAME "ToneMapper")
makeRenderPassLib(NAME "ImageLoader")
makeRenderPassLib(NAME "ForwardLightingPass")
makeRenderPassLib(NAME "SkyBox")
makeRenderPassLib(NAME "PixelInspectorPass")
makeRenderPassLib(NAME "AccumulatePass")
makeRenderPassLib(NAME "Antialiasing")
makeRenderPassLib(NAME "ErrorMeasurePass")
makeRenderPassLib(NAME "DebugPasses")
makeRenderPassLib(NAME "BSDFViewer")
makeRenderPassLib(NAME "GBuffer" DEPS /home/max/dev/Falcor/build/lib/passes/DepthPass_pass.dso)
makeRenderPassLib(NAME "SSAO" DEPS /home/max/dev/Falcor/build/lib/passes/Utils_pass.dso)
makeRenderPassLib(NAME "CSM" DEPS /home/max/dev/Falcor/build/lib/passes/Utils_pass.dso)