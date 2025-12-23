include(FindPkgConfig)
include(${PROJECT_SOURCE_DIR}/third_party/external_versions.cmake)

if(NOT EXTERNALS_INSTALL_DIR)
    set(EXTERNALS_INSTALL_DIR ${PROJECT_SOURCE_DIR}/deps/${CMAKE_BUILD_TYPE})
endif()

set(DEPS_DIR "${EXTERNALS_INSTALL_DIR}")


file( GLOB SOURCES
    ./logging.cpp
    ./lru_cache.cpp
    ./ut_fsys.cpp
    ./ut_string.cpp
    ./ut_system.cpp
)

add_library( lava_utils_lib SHARED ${SOURCES} ${HEADERS} )

# Find boost
set(BOOST_ROOT ${DEPS_DIR})

if(WIN32)
    add_definitions(-DBOOST_LOG_DYN_LINK)
    add_definitions(-DBOOST_USE_WINAPI_VERSION=0x0A00)
    set(Boost_USE_STATIC_LIBS OFF)
endif()

find_package( Boost ${EXTERNAL_BOOST_VERSION} COMPONENTS log thread filesystem system EXACT REQUIRED)
target_include_directories(lava_utils_lib PUBLIC ${Boost_INCLUDE_DIRS})

# CPUinfo

if(WIN32)
  set(PkgConfigWin32_BINDIR ${EXTERNALS_INSTALL_DIR}/bin)
  find_package(PkgConfigWin32)
  set(PKG_CONFIG_EXECUTABLE ${EXTERNALS_INSTALL_DIR}/bin/pkg-config.exe)
endif()

find_package(PkgConfig)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${EXTERNALS_INSTALL_DIR}/lib/pkgconfig")
pkg_check_modules(CpuInfo REQUIRED IMPORTED_TARGET libcpuinfo)

if(WIN32)
    add_definitions(-DBOOST_ALL_DYN_LINK)
    set_target_properties(lava_utils_lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
else()
    add_definitions(-DBOOST_LOG_DYN_LINK)
endif()

target_link_libraries(
    lava_utils_lib
    PUBLIC
        Boost::log
        Boost::filesystem
        Boost::thread
        Boost::system
        PkgConfig::CpuInfo
)

if(UNIX)
    install( 
        TARGETS lava_utils_lib 
        DESTINATION lib 
    )
elseif(WIN32)
    install( 
        TARGETS lava_utils_lib 
        RUNTIME DESTINATION bin 
        ARCHIVE DESTINATION lib
    )
    install(
        IMPORTED_RUNTIME_ARTIFACTS 
            Boost::log
            Boost::filesystem
            Boost::thread
            Boost::system
    )
endif()
