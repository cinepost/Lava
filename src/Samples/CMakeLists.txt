add_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")

# Python
find_package(PythonLibs 3.7 REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

# This function builds render pass library
function (makeFalcorSample sample_dir)
	file( GLOB_RECURSE SOURCES
		./${sample_dir}/*.cpp
    )

    set( SAMPLE_EXEC ${sample_dir})
    add_executable ( ${SAMPLE_EXEC} ${SOURCES} ${HEADERS} )

    target_link_libraries(
    	${SAMPLE_EXEC}
    	falcor_lib
    	${PYTHON_LIBRARIES}
    )

    set_target_properties( ${SAMPLE_EXEC} PROPERTIES 
  		CXX_STANDARD 17
  		CXX_STANDARD_REQUIRED ON
  		CXX_EXTENSIONS ON
	)

	# Copy/install tool data
	set(SAMPLE_DATA_DIR ${sample_dir}/Data)
	if(EXISTS ${SAMPLE_DATA_DIR})
		file(
    		COPY ${SAMPLE_DATA_DIR}
    		DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} 
		)
	endif()

	# Copy/install all needed shaders
	set( SHADERS_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders/Samples)
	file(MAKE_DIRECTORY ${SHADERS_OUTPUT_DIRECTORY})
	file(
    	COPY ${sample_dir}
    	DESTINATION ${SHADERS_OUTPUT_DIRECTORY} 
    	FILES_MATCHING
        	PATTERN "*.slang*"
	)

endfunction()

# Now make passes
makeFalcorSample("ModelViewer")
makeFalcorSample("ShaderToy")