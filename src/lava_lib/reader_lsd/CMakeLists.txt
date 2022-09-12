file( GLOB SOURCES
    ./*.cpp
)

add_library( reader_lsd_lib SHARED ${SOURCES} ${HEADERS} )

# Find Boost libs
find_package( Boost 1.73.0 COMPONENTS system filesystem REQUIRED )
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS}) 
endif()

include_directories( ${Boost_INCLUDE_DIRS} )

find_package(Python3 REQUIRED COMPONENTS Development)
target_include_directories(reader_lsd_lib PUBLIC ${Python3_INCLUDE_DIRS})

# We use Boost static libs
add_definitions( -DBOOST_LOG_DYN_LINK )

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
