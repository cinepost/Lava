file( GLOB SOURCES
    ./*.cpp
)

# Find Boost libs
find_package( Boost COMPONENTS system filesystem REQUIRED )
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS}) 
endif()

include_directories( ${Boost_INCLUDE_DIRS} )

# We use Boost static libs
add_definitions( -DBOOST_LOG_DYN_LINK )

add_library( reader_lsd_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    reader_lsd_lib
    reader_bgeo_lib
    ${Boost_LIBRARIES}
)

if(UNIX)
    install(TARGETS reader_lsd_lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    )
endif()
