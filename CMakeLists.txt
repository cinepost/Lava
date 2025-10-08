cmake_minimum_required( VERSION 3.21 )

if(UNIX)
  set(CMAKE_C_COMPILER gcc-11)
  set(CMAKE_CXX_COMPILER g++-11)
endif()

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS ON )

set( PROJECT_VERSION_MAJOR 0 )
set( PROJECT_VERSION_MINOR 1 )
set( PROJECT_VERSION_PATCH 001 )

if(WIN32)
  #file(REMOVE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/FindTBB.cmake)
  set( PKG_CONFIG_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pkgconfigwin32/pkg-config.exe)
endif()

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
endif()

# Install path
if(CMAKE_INSTALL_PREFIX)
  set(linuxDefault ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr/local")
  set(windowsDefault ${CMAKE_INSTALL_PREFIX} STREQUAL "C:\\Program Files (x86)")
else()
  set(linuxDefault TRUE)
  set(windowsDefault TRUE)
endif()


if(UNIX)
  set( DEFAULT_CMAKE_INSTALL_ROOT_PATH "/opt" )
elseif(WIN32)
  set( DEFAULT_CMAKE_INSTALL_ROOT_PATH "D:\\work" )
else()
  set( DEFAULT_CMAKE_INSTALL_ROOT_PATH "/Applications" )
endif()


if(UNIX)
  set( DEFAULT_CMAKE_INSTALL_PREFIX "${DEFAULT_CMAKE_INSTALL_ROOT_PATH}/lava" )
elseif(WIN32)
  set( DEFAULT_CMAKE_INSTALL_PREFIX "${DEFAULT_CMAKE_INSTALL_ROOT_PATH}\\lava")
else()
  set( DEFAULT_CMAKE_INSTALL_PREFIX "${DEFAULT_CMAKE_INSTALL_ROOT_PATH}/Lava" )
endif()

# Override default install path
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR ${linuxDefault} OR ${windowsDefault})
  set( CMAKE_INSTALL_PREFIX ${DEFAULT_CMAKE_INSTALL_PREFIX} CACHE PATH "Lava install prefix" FORCE)
endif()

message("Install path: ${CMAKE_INSTALL_PREFIX}")

if(UNIX)
  set(DEFAULT_HFS_PATH "/opt/hfs20.5")
  set(DEFAULT_HIH_PATH "$ENV{HOME}/houdini20.5")
else() # WIN
  set(USER_DOCUMENTS_PATH "$ENV{USERPROFILE}/Documents")
  set(DEFAULT_HFS_PATH "C:/Program Files/Side Effects Software/Houdini 20.5.332")
  set(DEFAULT_HIH_PATH "${USER_DOCUMENTS_PATH}/houdini20.5")
endif()

if (NOT DEFINED ENV{HFS})
  if(WIN32)
    message( WARNING "Houdini HFS environment variable not set. Using default path: ${DEFAULT_HFS_PATH}")
    set(ENV{HFS} ${DEFAULT_HFS_PATH})
  else()
    message( FATAL_ERROR "Houdini HFS environment variable not set. Source .houdini_setup first!")
  endif()
endif()

if (NOT DEFINED ENV{HIH})
  message( WARNING "Houdini HIH environment variable not set. Using defalut path: ${DEFAULT_HIH_PATH}")
  set(ENV{HIH} ${DEFAULT_HIH_PATH})
endif()

# Set up houdini hdk and installation paths
set(HOUDINI_ROOT $ENV{HFS})
set(HOUDINI_USER $ENV{HIH})

if(WIN32)
  add_definitions(
    -DNOMINMAX
    -D_USE_MATH_DEFINES
    -DWIN32
    # -D_WIN32
    # -DUNICODE
    # -DAMD64
    # -DSIZEOF_VOID_P=8
    -DHAVE_SNPRINTF
    -D_SCL_SECURE_NO_WARNINGS
    -D_CRT_SECURE_NO_WARNINGS
    -D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
endif()

message("Houdini: ${HOUDINI_ROOT}")

# Handle superbuild first
option (USE_SUPERBUILD "Whether or not a superbuild should be invoked" ON)

if(USE_SUPERBUILD)
  project (SUPERBUILD NONE)
  # execute the superbuild (this script will be invoked again without the USE_SUPERBUILD option this time)
  include (cmake/SuperBuild.cmake)
  return()
else()
  # Basic information about project
  set( PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} )
  project(Lava VERSION ${PROJECT_VERSION})
endif()

# Version generators
include (cmake/GenerateLavaVersionCpp.cmake)
include (cmake/GenerateLavaVersionPython.cmake)

# Add folder where are supportive functions
set( CMAKE_MODULE_PATH 
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake 
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/defaults
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/macros
  ${HOUDINI_ROOT}/toolkit/cmake
)

set( CMAKE_INCLUDE_CURRENT_DIR ON )

# Set PROJECT_VERSION_PATCH and PROJECT_VERSION_TWEAK to 0 if not present, needed by add_project_meta
#fix_project_version()

# Set additional project information
set( COMPANY "Parovoz" )
set( COPYRIGHT "Copyright (c) 2020 Parovoz" )
set( IDENTIFIER "com.parovoz" )

# Set python virtualenv for USD
if(UNIX)
set( PYTHON_EXECUTABLE "python3.8")
endif()

if( APPLE )
  if(POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)  # CMake 3.9+: `RPATH` settings on macOS do not affect `install_name`.
  endif()
endif()

# Specify build paths
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin )

add_definitions(-DLAVA_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-D_DEBUG)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
endif()

if(WIN32)
add_compile_definitions(
  $<$<CONFIG:RELEASE>:NDEBUG>
  # $<$<CONFIG:DEBUG>:DEBUG>
  $<$<CONFIG:DEBUG>:_DEBUG>
  $<$<CONFIG:DEBUG>:Py_DEBUG>
  # $<$<CONFIG:DEBUG>:BOOST_ALL_NO_LIB>
  # $<$<CONFIG:DEBUG>:TBB_USE_DEBUG=0>
  # $<$<CONFIG:DEBUG>:__TBB_LIB_NAME=tbb.lib>
  # $<$<CONFIG:DEBUG>:__TBB_NO_IMPLICIT_LINKAGE=1>
)
endif()

set(FALCOR_API_BACKEND "GFX" CACHE STRING "Rendering API")

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
  ADD_DEFINITIONS(/W3 /std:c++17 /MP /wd4005 /wd4068)
  add_compile_options($<$<CONFIG:DEBUG>:/bigobj>)
  add_compile_options(/utf-8)
elseif ( UNIX ) #(CMAKE_COMPILER_IS_GNUCXX)

  set(CMAKE_CXX_FLAGS "-Wall -Wextra")
  
  set(CMAKE_C_FLAGS_DEBUG "-g -O0")
  set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

  set(CMAKE_C_FLAGS_RELEASE "-O3")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3")


  ADD_DEFINITIONS(-Wall -fPIC -Wfatal-errors -D_GLIBCXX_USE_CXX11_ABI=0) # -Wno-reorder -pedantic
else ()
  MESSAGE(FATAL_ERROR "CMakeLists.txt has not been tested/written for your compiler.")
endif ()

set( FALCOR_INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( CMAKE_THIRD_PARTY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
include_directories(
  ${FALCOR_INCLUDE_ROOT}
  ${FALCOR_INCLUDE_ROOT}/Falcor
  ${FALCOR_INCLUDE_ROOT}/Falcor/Core

  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;${CMAKE_INSTALL_PREFIX}/lib/3rdparty")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


#------------------------------------------
# External Projects
#------------------------------------------

set( FALCOR_BUILD_LIBS_AS_FRAMEWORKS true )


# Lava version builder that produces version.cpp, plus
# a dummy output that's not actually produced, in order
# to force version.cmake to always be re-run before the build
ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
         ${CMAKE_CURRENT_BINARY_DIR}/_version.cpp
  COMMAND ${CMAKE_COMMAND} -P
          version.cmake
)

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
add_subdirectory( src )
add_subdirectory( contrib )

install(FILES src/changelog.txt DESTINATION .)

#------------------------------------------
# Install third party dependencies
#------------------------------------------

if(NOT EXTERNALS_INSTALL_DIR)
  set(EXTERNALS_INSTALL_DIR ${PROJECT_SOURCE_DIR}/deps/${CMAKE_BUILD_TYPE})
endif()

# third party libs
if(WIN32)
  install(
    DIRECTORY 
      ${EXTERNALS_INSTALL_DIR}/lib/ 
      ${EXTERNALS_INSTALL_DIR}/bin/ 
    DESTINATION 
      bin
    FILES_MATCHING PATTERN "*.dll"
    PATTERN "*.lib" EXCLUDE
    PATTERN "cmake" EXCLUDE
    PATTERN "pkgconfig" EXCLUDE
    PATTERN "libpng" EXCLUDE
  )
else()
  install(
    DIRECTORY 
      ${EXTERNALS_INSTALL_DIR}/lib/ 
    DESTINATION 
      lib/3rdparty
    FILES_MATCHING PATTERN "*.so*"
    PATTERN "cmake" EXCLUDE
    PATTERN "pkgconfig" EXCLUDE
    PATTERN "libpng" EXCLUDE
  )
endif()

# third additional shaders and dependencies
#install(DIRECTORY ${EXTERNALS_INSTALL_DIR}/shaders/ DESTINATION shaders)

# CPack

include(InstallRequiredSystemLibraries)

#set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/License.txt")

set(CPACK_PACKAGE_NAME "Lava")
set(CPACK_PACKAGE_VENDOR "PAROVOZ")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "lava")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lava - Vulkan Photo Relistic Renderer")

if(WIN32)
  set(CPACK_GENERATOR "NSIS")
  set(CPACK_NSIS_INSTALL_ROOT ${DEFAULT_CMAKE_INSTALL_ROOT_PATH})
  set(CPACK_NSIS_MODIFY_PATH ON)
else()
  set(CPACK_GENERATOR "TGZ")
  set(CPACK_SOURCE_GENERATOR "TGZ")
endif()

include(CPack)
