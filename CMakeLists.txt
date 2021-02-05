cmake_minimum_required( VERSION 3.5 )

set(CMAKE_C_COMPILER gcc-7)
set(CMAKE_CXX_COMPILER g++-7)

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS ON )

set( PROJECT_VERSION_MAJOR 0 )
set( PROJECT_VERSION_MINOR 0 )
set( PROJECT_VERSION_PATCH 001 )

# Basic information about project
set( PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} )
project( Falcor VERSION ${PROJECT_VERSION} )

# Set up houdini hdk and installation paths
set(HOUDINI_HOME "/opt/houdini18.5")
set(HOUDINI_USER "$ENV{HOME}/houdini18.5")

list( APPEND CMAKE_PREFIX_PATH "${HOUDINI_HOME}/toolkit/cmake" )


# Add folder where are supportive functions
set( CMAKE_MODULE_PATH 
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake 
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/defaults
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/macros
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm/cmake
  ${CMAKE_INSTALL_PREFIX}/3rdparty/lib/cmake/assimp-5.0
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/OpenEXR/cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/OpenImageIO/src/cmake
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
  set( CMAKE_INSTALL_PREFIX "/Applications/Falcor/Falcor" )
elseif(UNIX)
  set( CMAKE_INSTALL_PREFIX "/opt/lava/" )
endif()

# Handle build mode
if( NOT CMAKE_BUILD_TYPE )
  SET( CMAKE_BUILD_TYPE "DEBUG" )
  #SET(CMAKE_BUILD_TYPE "RELEASE")
  #SET(CMAKE_BUILD_TYPE "RELWITHDEBINFO")
  #SET(CMAKE_BUILD_TYPE "MINSIZEREL")
  message("CMAKE_BUILD_TYPE not set! Using default ${CMAKE_BUILD_TYPE} mode.")
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


if (CMAKE_BUILD_TYPE STREQUAL "DEBUG")
  add_definitions(-D_DEBUG)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
endif ()


if ( MSVC )
  ADD_DEFINITIONS(/W3)
elseif ( UNIX ) #(CMAKE_COMPILER_IS_GNUCXX)
  ADD_DEFINITIONS(-Wall -fPIC -Wfatal-errors -g3) # -fPIC -Wno-reorder -pedantic
else ()
  MESSAGE(FATAL_ERROR "CMakeLists.txt has not been tested/written for your compiler.")
endif ()

set ( FALCOR_INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src )
include_directories(
  ${FALCOR_INCLUDE_ROOT}
  ${FALCOR_INCLUDE_ROOT}/Falcor
  ${FALCOR_INCLUDE_ROOT}/Falcor/Core
  ${FALCOR_INCLUDE_ROOT}/Externals

  ${CMAKE_INSTALL_PREFIX}/3rdparty/include

  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rapidjson
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Externals/GLM
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
# Documentation
#------------------------------------------

set( FALCOR_BUILD_LIBS_AS_FRAMEWORKS true )

# Now setup targets

# thirt party libs
#add_subdirectory( third_party )

# Our stuff
add_subdirectory( src )
#add_subdirectory( contrib )


#install(
#    DIRECTORY
#    ${CMAKE_CURRENT_SOURCE_DIR}/tests
#    ${CMAKE_CURRENT_SOURCE_DIR}/contrib
#    DESTINATION ${CMAKE_INSTALL_PREFIX}
#    USE_SOURCE_PERMISSIONS
#    FILES_MATCHING PATTERN "*"
#)