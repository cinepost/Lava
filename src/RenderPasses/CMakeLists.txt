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

    set( PASS_LIB ${pass_dir}_lib)

    add_library( ${PASS_LIB} SHARED ${SOURCES} ${HEADERS} )

    target_link_libraries(
    	${PASS_LIB}
    	falcor_lib
    	${PYTHON_LIBRARIES}
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