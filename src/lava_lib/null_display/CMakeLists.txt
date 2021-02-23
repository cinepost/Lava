file( GLOB SOURCES 
	./*.cpp
) 

include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}
)

if (NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set (flags_1 "-Wno-overloaded-virtual")
else()
  set (flags_1 "-MP /wd4251")
endif()


add_library ( 
	null_display 
	SHARED ${SOURCES} 
)

target_link_libraries( 
	null_display
)

set_target_properties( null_display PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS ON
	PREFIX "d_"
	SUFFIX ".so"
	OUTPUT_NAME "null"
)


install(
	TARGETS null_display
	LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/etc
)

install(
	FILES ${GLEW_LIBRARIS}
	DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)
