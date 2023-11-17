file( GLOB SOURCES
    ./*.cpp
)

list( APPEND CMAKE_PREFIX_PATH "${HOUDINI_ROOT}/toolkit/cmake" )
find_package( Houdini REQUIRED )

include_directories(
  ${HOUDINI_ROOT}/toolkit/include
)

if(WIN32)
#set( HOU_LIBS ${HOUDINI_ROOT}/custom/houdini/dsolib/libUT.lib)
add_definitions(-DAMD64 -DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P})
else()
#set( HOU_LIBS ${HOUDINI_ROOT}/dsolib/libHoudiniUT.so)
add_definitions(-DAMD64 -DLINUX -DUSE_PTHREADS -DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P})
endif()

set( library_name IMG_LavaLTX )
add_library( ${library_name} SHARED ${SOURCES} ${HEADERS} )


# Link against the Houdini libraries, and add required include directories and compile definitions.
target_link_libraries(
	${library_name}
	PUBLIC
	Houdini
	#${HOU_LIBS}
	falcor_lib 
	lava_utils_lib
)

set_target_properties(${library_name} PROPERTIES PREFIX "")

# Configure several common target properties, such as its output directory.
houdini_configure_target( ${library_name} INSTDIR ${CMAKE_INSTALL_PREFIX}/houdini/dso)