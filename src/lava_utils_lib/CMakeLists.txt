file( GLOB SOURCES
    ./*.cpp
)

set(EXTERNALS_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../third_party/external/)

add_library( lava_utils_lib SHARED ${SOURCES} ${HEADERS} )

# Find boost
set(Boost_NO_SYSTEM_PATHS ON)
set(BOOST_ROOT ${EXTERNALS_BUILD_DIR})
find_package( Boost 1.73.0 COMPONENTS log thread filesystem system REQUIRED)
target_include_directories(lava_utils_lib PUBLIC ${Boost_INCLUDE_DIRS})

# we will use static libs
add_definitions(-DBOOST_LOG_DYN_LINK)

target_link_libraries(
    lava_utils_lib
    PUBLIC
        Boost::log
        Boost::filesystem
        Boost::thread
        Boost::system
)

if(UNIX)
    install( TARGETS lava_utils_lib DESTINATION "lib" )
endif()