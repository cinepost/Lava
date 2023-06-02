include(${PROJECT_SOURCE_DIR}/third_party/external_versions.cmake)


file( GLOB SOURCES
    ./*.cpp
)
set(DEPS_DIR "${EXTERNALS_INSTALL_DIR}/")
add_library( lava_utils_lib SHARED ${SOURCES} ${HEADERS} )

# Find boost
set(BOOST_ROOT ${DEPS_DIR})
if(WIN32)
set(Boost_USE_STATIC_LIBS OFF)
endif()
find_package( Boost ${EXTERNAL_BOOST_VERSION} COMPONENTS log thread filesystem system EXACT REQUIRED)
target_include_directories(lava_utils_lib PUBLIC ${Boost_INCLUDE_DIRS})

if(WIN32)
add_definitions(-DBOOST_ALL_DYN_LINK)
set_target_properties(lava_utils_lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
else()
# we will use static libs
add_definitions(-DBOOST_LOG_DYN_LINK)
endif()

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

if(UNIX OR WIN32)
    install( TARGETS lava_utils_lib DESTINATION "lib" )
endif()
