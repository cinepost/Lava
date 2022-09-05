cmake_minimum_required( VERSION 3.20 )

set(CMAKE_C_COMPILER gcc-7)
set(CMAKE_CXX_COMPILER g++-7)

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS ON )

set( PROJECT_VERSION_MAJOR 0 )
set( PROJECT_VERSION_MINOR 0 )
set( PROJECT_VERSION_PATCH 001 )

#### Handle build mode
# Set a default build type if none was specified
set(default_build_type "Release")
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  set(default_build_type "Debug")
endif()

if( NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES )
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)

  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

option (USE_PROFILING "Enable/Disable renderer performance profiler." OFF)
if (USE_PROFILING)
  add_definitions(-DFALCOR_ENABLE_PROFILER)
endif ()

# Handle superbuild first
option (USE_SUPERBUILD "Whether or not a superbuild should be invoked" ON)

if(USE_SUPERBUILD)
  project (SUPERBUILD NONE)
  # execute the superbuild (this script will be invoked again without the
  # USE_SUPERBUILD option this time)
  include (cmake/SuperBuild.cmake)
  return()
else()
  # Basic information about project
  set( PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} )
  project( Falcor VERSION ${PROJECT_VERSION} )
endif()

# ccache
include(cmake/CPM.cmake)

CPMAddPackage(
  NAME Ccache.cmake
  GITHUB_REPOSITORY TheLartians/Ccache.cmake
  VERSION 1.2
)

# Set up houdini hdk and installation paths
set(HOUDINI_HOME "/opt/hfs19.0.622")
set(HOUDINI_USER "$ENV{HOME}/houdini19.0")

# TODO move to houdini related subproject
#list( APPEND CMAKE_PREFIX_PATH "${HOUDINI_HOME}/toolkit/cmake" )


# Add folder where are supportive functions
set( CMAKE_MODULE_PATH 
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake 
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/defaults
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/macros
)
set( CMAKE_INCLUDE_CURRENT_DIR ON )

# Set PROJECT_VERSION_PATCH and PROJECT_VERSION_TWEAK to 0 if not present, needed by add_project_meta
#fix_project_version()

# Set additional project information
set( COMPANY "Parovoz" )
set( COPYRIGHT "Copyright (c) 2020 Parovoz" )
set( IDENTIFIER "com.parovoz" )

# Set python virtualenv for USD
set( PYTHON_EXECUTABLE "python3.7")

if( APPLE )
  if(POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)  # CMake 3.9+: `RPATH` settings on macOS do not affect `install_name`.
  endif()
endif()

if(APPLE)
  set(CMAKE_MACOSX_RPATH 1)
  set( CMAKE_INSTALL_PREFIX "/Applications/Lava" )
elseif(UNIX)
  set( CMAKE_INSTALL_PREFIX "/opt/lava/" )
endif()

# Specify build paths
set( BUILD_SUBDIR "")
string(TOLOWER ${CMAKE_BUILD_TYPE} BUILD_SUBDIR)

set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${BUILD_SUBDIR}/lib )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${BUILD_SUBDIR}/lib )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${BUILD_SUBDIR}/bin )

add_definitions(-DLAVA_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}")

if (WIN32 OR APPLE)
  if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # We don't want to install in default system location, install is really for the SDK, so call it that
    set(CMAKE_INSTALL_PREFIX "${PROJECT_BINARY_DIR}/sdk" CACHE PATH "Falcor install prefix" FORCE )
  endif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
endif(WIN32 OR APPLE)

string( TOLOWER ${CMAKE_BUILD_TYPE} build_type )
if (build_type STREQUAL "debug")
  add_definitions(-D_DEBUG)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
endif ()

set(FALCOR_API_BACKEND "Default" CACHE STRING "Vulkan")

option (PROFILING "Enable/Disable renderer performance profiler." OFF)
option (PROFILING_LOG "Enable/Disable renderer performance profiler logging." OFF)

if (PROFILING)
  add_definitions(-D_PROFILING_ENABLED)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_PROFILING_ENABLED")
endif ()

if (PROFILING_LOG)
  add_definitions(-D_PROFILING_LOG)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_PROFILING_LOG")  
endif ()

if ( MSVC )
  ADD_DEFINITIONS(/W3)
elseif ( UNIX ) #(CMAKE_COMPILER_IS_GNUCXX)
  ADD_DEFINITIONS(-Wall -fPIC -Wfatal-errors -g3 -D_GLIBCXX_USE_CXX11_ABI=0) # -Wno-reorder -pedantic
else ()
  MESSAGE(FATAL_ERROR "CMakeLists.txt has not been tested/written for your compiler.")
endif ()

set( FALCOR_INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( CMAKE_THIRD_PARTY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
include_directories(
  ${FALCOR_INCLUDE_ROOT}
  ${FALCOR_INCLUDE_ROOT}/Falcor
  ${FALCOR_INCLUDE_ROOT}/Falcor/Core
  ${FALCOR_INCLUDE_ROOT}/Externals

  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#------------------------------------------
# External Projects
#------------------------------------------

set( FALCOR_BUILD_LIBS_AS_FRAMEWORKS true )


#------------------------------------------
# Rendering backend configuration
#------------------------------------------

string( TOLOWER ${FALCOR_API_BACKEND} backend_type )
if(backend_type STREQUAL "gfx")
  add_definitions(-D_FLACOR_API_BACKEND_GFX)
endif()

#------------------------------------------
# Our stuff
#------------------------------------------

#if (CMAKE_BUILD_TYPE STREQUAL "Release")
#  set(EXTERNALS_INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/release)
#  list(PREPEND CMAKE_PREFIX_PATH "${EXTERNALS_INSTALL_DIR}")
#else()
#  set(EXTERNALS_INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/debug)
#  list(PREPEND CMAKE_PREFIX_PATH "${EXTERNALS_INSTALL_DIR}")
#endif()

add_subdirectory( src )
add_subdirectory( contrib )
