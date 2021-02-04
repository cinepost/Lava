file( GLOB SOURCES
    ./*.cpp
)

# find boost libs
find_package( Boost COMPONENTS log log_setup thread filesystem system REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )

# we will use static libs
add_definitions(-DBOOST_LOG_DYN_LINK=1)

add_library( lava_utils_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    lava_utils_lib
    ${Boost_LOG_SETUP_LIBRARY}
    ${Boost_LOG_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
)

if(UNIX)
    install( TARGETS lava_utils_lib DESTINATION "lib" )
endif()