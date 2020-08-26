file( GLOB SOURCES
    ./*.cpp
)

# Python
find_package( PythonLibs 3.7 REQUIRED )
include_directories( ${PYTHON_INCLUDE_DIRS} )

# FreeImage
find_package(FreeImage REQUIRED)
include_directories( ${FREEIMAGE_INCLUDE_DIRS} )

# Find Boost libs
find_package( Boost COMPONENTS system filesystem REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )

# We use Boost static libs
add_definitions( -DBOOST_LOG_DYN_LINK )


add_library( lava_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    lava_lib
    falcor_lib
    lava_utils_lib
)


if(APPLE)
    if( COPPER_BUILD_LIBS_AS_FRAMEWORKS )
	#falcor_config_framework( lava_lib )

    # Framework is called 'Falcor'
    set_target_properties( lava_lib PROPERTIES OUTPUT_NAME "Falcor" )
    install( TARGETS lava_lib DESTINATION "Frameworks" )
  else()
    install( TARGETS lava_lib DESTINATION "lib" )
  endif()

elseif(UNIX)
    install( TARGETS lava_lib DESTINATION lib )
endif()
