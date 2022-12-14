include_directories( 
    ${EXTERNALS_INSTALL_DIR}
)

set (SOURCES 
	./lava_cmd.cpp
)

# Find BOOST
find_package( Boost 1.73.0 COMPONENTS program_options log EXACT REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )

# Executable 
add_executable ( lava_cmd ${SOURCES} )
set_target_properties( lava_cmd PROPERTIES OUTPUT_NAME "lava-bin" )
 
target_link_libraries( lava_cmd
	gfx_lib
	lava_lib
	lava_utils_lib
	falcor_lib  
)

if(USE_MEGALIB)
	target_link_libraries( lava_cmd
		mega_lib
	)
else()
	add_definitions(-DBOOST_LOG_DYN_LINK)
	target_link_libraries( lava_cmd
		Boost::log 
		Boost::program_options 
	)
endif()

install(
	TARGETS lava_cmd 
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
)

install(
	PROGRAMS
		bin/lava
	DESTINATION 
		${CMAKE_INSTALL_PREFIX}/bin/
)

set(LAVA_INST_DIR "${CMAKE_INSTALL_PREFIX}")
configure_file(bin/lava_app_init.sh.in ${CMAKE_CURRENT_BINARY_DIR}/lava_app_init.sh.install @ONLY)

install(
    PROGRAMS
    	${CMAKE_CURRENT_BINARY_DIR}/lava_app_init.sh.install
    RENAME
    	lava_app_init.sh
    DESTINATION
    	${CMAKE_INSTALL_PREFIX}/bin/
)

install(DIRECTORY conf DESTINATION ${CMAKE_INSTALL_PREFIX})