set (SOURCES 
	./d_houdini.cpp
)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# Find Boost libs
find_package( Boost COMPONENTS log REQUIRED )

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