set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set (SOURCES 
	./lava_cmd.cpp
)

# Find BOOST
find_package( Boost COMPONENTS program_options log REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )
# we will use static libs
add_definitions(-DBOOST_LOG_DYN_LINK)

# Python
find_package( PythonLibs 3.7 REQUIRED )
include_directories( ${PYTHON_INCLUDE_DIRS} )

# RPATH 
SET(CMAKE_SKIP_BUILD_RPATH  FALSE) # use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) # when building, don't use the install RPATH already (but later on when installing)
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE) # add the automatically determined parts of the RPATH which point to directories outside the build tree to the install RPATH

# Executable 
add_executable ( lava_cmd ${SOURCES} )
add_dependencies( lava_cmd ThirdParty_Imath)
set_target_properties( lava_cmd PROPERTIES OUTPUT_NAME "lava-bin" )
 
target_link_libraries( lava_cmd
	lava_lib
	lava_utils_lib
	falcor_lib 
	Boost::log 
	Boost::program_options 
)

install(
	TARGETS lava_cmd 
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
)

install(
	DIRECTORY bin
	DESTINATION ${CMAKE_INSTALL_PREFIX}
  	USE_SOURCE_PERMISSIONS
)

install(DIRECTORY conf DESTINATION ${CMAKE_INSTALL_PREFIX})