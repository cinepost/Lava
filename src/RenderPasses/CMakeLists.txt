set( PASS_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/passes )

add_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")

# external passes (can also be made built-in)
add_definitions(-DBUILD_DEPTH_PASS=TRUE)
add_definitions(-DBUILD_GAUSSIAN_PASS=TRUE)

# Python
find_package(PythonLibs 3.7 REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

# This function builds render pass library
function (makeRenderPassLib pass_dir)
	file( GLOB_RECURSE SOURCES
		./${pass_dir}/*.cpp
    )

    set( PASS_LIB ${pass_dir}_pass)

    add_library( ${PASS_LIB} SHARED ${SOURCES} ${HEADERS} )

    set_target_properties( ${PASS_LIB} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${PASS_LIBRARY_OUTPUT_DIRECTORY}
    )

    #ifdef(UNIX)
    set_target_properties( ${PASS_LIB} PROPERTIES PREFIX "" )
    set_target_properties( ${PASS_LIB} PROPERTIES SUFFIX ".dso" )
    #endif

    target_link_libraries(
    	${PASS_LIB}
    	falcor_lib
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
    add_custom_command(
        TARGET ${PASS_LIB} 
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${PASS_LIB}> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Passes
    )

endfunction()

# Now make passes
makeRenderPassLib("GBuffer")
makeRenderPassLib("DepthPass")
makeRenderPassLib("BlitPass")
makeRenderPassLib("SSAO")
makeRenderPassLib("ToneMapper")
makeRenderPassLib("ImageLoader")
makeRenderPassLib("ForwardLightingPass")
makeRenderPassLib("SkyBox")
makeRenderPassLib("PixelInspectorPass")
makeRenderPassLib("AccumulatePass")
makeRenderPassLib("Antialiasing")
makeRenderPassLib("ErrorMeasurePass")
makeRenderPassLib("CSM")
makeRenderPassLib("DebugPasses")
makeRenderPassLib("BSDFViewer")