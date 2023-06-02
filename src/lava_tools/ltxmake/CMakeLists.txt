include(${PROJECT_SOURCE_DIR}/third_party/external_versions.cmake)

set (SOURCES 
	./ltxmake.cpp
)

# Find BOOST
if(WIN32)
set(BOOST_ROOT ${DEPS_DIR})
set(Boost_USE_STATIC_LIBS OFF)
find_package( Boost ${EXTERNAL_BOOST_VERSION} COMPONENTS filesystem program_options log thread EXACT REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )
add_definitions(-DBOOST_ALL_DYN_LINK)
else()
find_package( Boost ${EXTERNAL_BOOST_VERSION} COMPONENTS filesystem program_options log EXACT REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )
add_definitions(-DBOOST_LOG_DYN_LINK)
endif()

# Executable 
add_executable ( ltxmake ${SOURCES} )
set_target_properties( ltxmake PROPERTIES OUTPUT_NAME "ltxmake-bin" )
 
target_link_libraries( ltxmake
	lava_lib
	lava_utils_lib
	falcor_lib 
	Boost::log 
	Boost::program_options 
	Boost::filesystem
)

if(WIN32)
target_link_libraries( ltxmake
	Boost::thread
)
endif()

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
