include(${PROJECT_SOURCE_DIR}/third_party/external_versions.cmake)


file( GLOB SOURCES
    ./*.cpp
)
set(DEPS_DIR "${EXTERNALS_INSTALL_DIR}/")
add_library( lava_utils_lib SHARED ${SOURCES} ${HEADERS} )

# Find boost
set(BOOST_ROOT ${DEPS_DIR})
find_package( Boost ${EXTERNAL_BOOST_VERSION} COMPONENTS log thread filesystem system EXACT REQUIRED)
target_include_directories(lava_utils_lib PUBLIC ${Boost_INCLUDE_DIRS})

# we will use static libs
add_definitions(-DBOOST_LOG_DYN_LINK)

if(USE_MEGALIB)
target_link_libraries(
    lava_utils_lib
    PUBLIC
        mega_lib
)
else()
target_link_libraries(
    lava_utils_lib
    PUBLIC
        Boost::log
        Boost::filesystem
        Boost::thread
        Boost::system
)
endif()

if(UNIX)
    install( TARGETS lava_utils_lib DESTINATION "lib" )
endif()