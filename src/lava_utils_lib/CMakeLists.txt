file( GLOB SOURCES
    ./*.cpp
)

set(EXTERNALS_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../third_party/external/)

# Find boost
set(Boost_NO_SYSTEM_PATHS ON)
set(BOOST_ROOT ${EXTERNALS_BUILD_DIR})
find_package( Boost 1.73.0 COMPONENTS log thread filesystem system REQUIRED )

include_directories( 
    ${Boost_INCLUDE_DIRS} 
)

# we will use static libs
#add_definitions(-DBOOST_LOG_DYN_LINK=1)

add_library( lava_utils_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    lava_utils_lib
    Boost::log
    Boost::filesystem
    Boost::thread
    Boost::system
)

if(UNIX)
    install( TARGETS lava_utils_lib DESTINATION "lib" )
endif()