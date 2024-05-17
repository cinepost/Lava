include(${PROJECT_SOURCE_DIR}/third_party/external_versions.cmake)


file( GLOB SOURCES
    ./*.cpp
)

add_library( reader_lsd_lib SHARED ${SOURCES} ${HEADERS} )

# Find Boost libs
if(WIN32)
set(BOOST_ROOT ${DEPS_DIR})
set(Boost_USE_STATIC_LIBS OFF)
find_package( Boost ${EXTERNAL_BOOST_VERSION} COMPONENTS system filesystem log json EXACT REQUIRED)
add_definitions(-DBOOST_ALL_DYN_LINK)
else()
find_package( Boost ${EXTERNAL_BOOST_VERSION} COMPONENTS system filesystem EXACT REQUIRED)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    add_definitions( -DBOOST_LOG_DYN_LINK )
endif()
endif()

include_directories( ${Boost_INCLUDE_DIRS} )

if(WIN32)
    message(STATUS "Using module to find Vulkan")
    find_package(Vulkan REQUIRED)

include_directories( ${Vulkan_INCLUDE_DIRS} ${EXTERNALS_INSTALL_DIR} ${EXTERNALS_INSTALL_DIR}/include )

# SLANG
find_library( SLANG_GFX_LIBRARY gfx PATHS ${DEPS_DIR}/lib )
endif()

find_package(Python3 REQUIRED COMPONENTS Development)
target_include_directories(reader_lsd_lib PUBLIC ${Python3_INCLUDE_DIRS})

if(WIN32)
    # set_target_properties(reader_lsd_lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    # target_link_options(reader_lsd_lib PRIVATE "/FORCE:UNRESOLVED")
endif()

target_link_libraries(
    reader_lsd_lib
    PUBLIC
    reader_bgeo_lib
    ${Boost_LIBRARIES}
)

if(WIN32)
target_link_libraries(
    reader_lsd_lib
    PUBLIC
    falcor_lib
    ${SLANG_GFX_LIBRARY}
    PRIVATE
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/lava_lib.lib
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/AccumulatePass.lib
)
endif()

if(UNIX OR WIN32)
    install(TARGETS reader_lsd_lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    )
endif()
