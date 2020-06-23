add_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")

# Python
find_package(PythonLibs 3.7 REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

# This function builds render pass library
function (makeFalcorTool tool_dir)
	file( GLOB_RECURSE SOURCES
		./${tool_dir}/*.cpp
    )

    set( TOOL_EXEC ${tool_dir})
    add_executable ( ${TOOL_EXEC} ${SOURCES} ${HEADERS} )

    target_link_libraries(
    	${TOOL_EXEC}
    	falcor_lib
    	${PYTHON_LIBRARIES}
    )

    set_target_properties( ${TOOL_EXEC} PROPERTIES 
  		CXX_STANDARD 17
  		CXX_STANDARD_REQUIRED ON
  		CXX_EXTENSIONS ON
	)

	# Copy/install tool data
	set(TOOL_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${tool_dir}/Data")
  if(IS_DIRECTORY ${TOOL_DATA_DIR})
		file(
    		COPY ${TOOL_DATA_DIR}
    		DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} 
		)
	endif()

  # Copy/install all needed shaders
  if( ${tool_dir} STREQUAL "FalcorTest" )
    message ("Copy FalcorTest shaders...")
    set( SHADERS_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders" )
    file( MAKE_DIRECTORY ${SHADERS_OUTPUT_DIRECTORY} )
    set( TEST_SHADERS_DIRECTORY "${tool_dir}/Tests" )
    file(
        COPY ${TEST_SHADERS_DIRECTORY}
        DESTINATION ${SHADERS_OUTPUT_DIRECTORY} 
        FILES_MATCHING
          PATTERN "*.slang*"
    )
  endif()
endfunction()

# Now make passes
makeFalcorTool("RenderGraphEditor")
makeFalcorTool("ImageCompare")
makeFalcorTool("FalcorTest")
