include(${PROJECT_SOURCE_DIR}/third_party/external_versions.cmake)

set (SOURCES 
	./d_houdini.cpp
)


# Find Boost libs
set(Boost_USE_MULTITHREADED ON)
find_package( Boost ${EXTERNAL_BOOST_VERSION} COMPONENTS log EXACT REQUIRED )

# OpenMP
find_package(OpenMP REQUIRED)

include_directories( 
	${CMAKE_SOURCE_DIR}
	${Boost_INCLUDE_DIRS} 
)

add_library( d_houdini SHARED ${SOURCES} )

#if(OpenMP_CXX_FOUND)
target_link_libraries(d_houdini PUBLIC ${Boost_LIBRARIES} OpenMP::OpenMP_CXX )

# don't prepend wrapper library name with lib
set_target_properties( d_houdini PROPERTIES PREFIX "" )

if (APPLE)
    set_target_properties( d_houdini PROPERTIES
    	SUFFIX ".so"
    )
endif (APPLE)
	
install(TARGETS d_houdini DESTINATION ${CMAKE_INSTALL_PREFIX}/etc)