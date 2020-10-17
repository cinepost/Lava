set (SOURCES 
	./d_houdini.cpp
)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# find boost libs
find_package( Boost COMPONENTS log python REQUIRED )
find_package( Boost COMPONENTS python27 )

include_directories( ${Boost_INCLUDE_DIRS} )

# define wrapper library
add_library( d_houdini SHARED ${SOURCES} )
target_link_libraries( d_houdini ${Boost_LIBRARIES} )

# don't prepend wrapper library name with lib
set_target_properties( d_houdini PROPERTIES PREFIX "" )

if (APPLE)
    set_target_properties( d_houdini PROPERTIES
    	SUFFIX ".so"
    )
endif (APPLE)
	
install(TARGETS d_houdini DESTINATION ${CMAKE_INSTALL_PREFIX}/etc)