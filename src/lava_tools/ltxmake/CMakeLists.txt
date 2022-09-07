set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set (SOURCES 
	./ltxmake.cpp
)

# Find BOOST
find_package( Boost COMPONENTS filesystem program_options log REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )
# we will use static libs
add_definitions(-DBOOST_LOG_DYN_LINK)

# RPATH 
SET(CMAKE_SKIP_BUILD_RPATH  FALSE) # use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) # when building, don't use the install RPATH already (but later on when installing)
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE) # add the automatically determined parts of the RPATH which point to directories outside the build tree to the install RPATH

# Executable 
add_executable ( ltxmake ${SOURCES} )
set_target_properties( ltxmake PROPERTIES OUTPUT_NAME "ltxmake-bin" )
 
target_link_libraries( 
	ltxmake
	lava_utils_lib
	falcor_lib 
	Boost::log 
	Boost::program_options 
	Boost::filesystem
)

install(
	TARGETS ltxmake 
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
)

install(
	PROGRAMS
		../bin/ltxmake
	DESTINATION 
		${CMAKE_INSTALL_PREFIX}/bin/
)
