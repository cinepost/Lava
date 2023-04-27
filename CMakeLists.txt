cmake_minimum_required( VERSION 3.21 )

# set(CMAKE_C_COMPILER gcc-7)
# set(CMAKE_CXX_COMPILER g++-7)

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
endif()

option (USE_MEGALIB "Build most of the third-party dependencies as a separate mega library." OFF)
if(USE_MEGALIB)
  set(Boost_USE_STATIC_RUNTIME ON)
endif()

option (COLLECT "Enable/Disable portable build dependencies collection." OFF)
if (COLLECT)
  # If we are going to collect dependecies, it's most likely to be a deployment build. So we are setting _DEPLOY_BUILD so we can make some optimisations later in the code.
  add_definitions(-D_DEPLOY_BUILD)
endif()

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
  project( Falcor VERSION ${PROJECT_VERSION} )
endif()

# Version generators
include (cmake/GenerateLavaVersionCpp.cmake)
include (cmake/GenerateLavaVersionPython.cmake)


# ccache
#include(cmake/CPM.cmake)

#CPMAddPackage(
#  NAME Ccache.cmake
#  GITHUB_REPOSITORY TheLartians/Ccache.cmake
#  VERSION 1.2
#)

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
# set( PYTHON_EXECUTABLE "python3.8")
# set( PYTHON_EXECUTABLE "python3.7")

if( APPLE )
  if(POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)  # CMake 3.9+: `RPATH` settings on macOS do not affect `install_name`.
  endif()
endif()

if(APPLE)
  set(CMAKE_MACOSX_RPATH 1)
  set( CMAKE_INSTALL_PREFIX "/Applications/Lava" )
elseif(UNIX)
  set( CMAKE_INSTALL_PREFIX "/opt/lava" )
elseif(WIN32)
  set( CMAKE_INSTALL_PREFIX "D:/Lava/install" )
endif()

message("CMAKE_INSTALL_PREFIX: " ${CMAKE_INSTALL_PREFIX})

# Specify build paths
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin )

add_definitions(-DLAVA_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}")

if (WIN32 OR APPLE)
  if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # We don't want to install in default system location, install is really for the SDK, so call it that
    set(CMAKE_INSTALL_PREFIX "${PROJECT_BINARY_DIR}/sdk" CACHE PATH "Falcor install prefix" FORCE )
  endif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
endif(WIN32 OR APPLE)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-D_DEBUG)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
# elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
#   add_definitions(-DNDEBUG)
endif ()

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

set(FALCOR_API_BACKEND "Vulkan" CACHE STRING "Rendering API")

# Set up houdini hdk and installation paths
if(WIN32)
  set(HOUDINI_USER "$ENV{USERPROFILE}/Documents/houdini19.5" CACHE PATH "Houdini home user path")
  set(HOUDINI_ROOT "C:/Program Files/Side Effects Software/Houdini 19.5.534" CACHE PATH "Houdini installation path")
  add_definitions(
    # -DNDEBUG
    -DNOMINMAX
    -D_USE_MATH_DEFINES
    -DWIN32
    -D_WIN32
    # -DUNICODE
    -DHAVE_SNPRINTF
    -D_SCL_SECURE_NO_WARNINGS
    -D_CRT_SECURE_NO_WARNINGS
    -D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
elseif()
  set(HOUDINI_USER "$ENV{HOME}/houdini19.5" CACHE PATH "Houdini home user path")
  set(HOUDINI_ROOT "/opt/hfs19.5" CACHE PATH "Houdini installation path")
endif()

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
  ADD_DEFINITIONS(/W3 /std:c++17 /MP /wd4005 /wd4068) #/
  add_compile_options($<$<CONFIG:DEBUG>:/bigobj>)
elseif ( UNIX ) #(CMAKE_COMPILER_IS_GNUCXX)
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
  ${FALCOR_INCLUDE_ROOT}/Externals

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

install(FILES src/changelog.txt DESTINATION ${CMAKE_INSTALL_PREFIX})

#------------------------------------------
# Install third party dependencies
#------------------------------------------

# third party libs
install(DIRECTORY ${EXTERNALS_INSTALL_DIR}/lib/ DESTINATION lib/3rdparty
        FILES_MATCHING PATTERN "*.so*"
        PATTERN "*.dll"
        PATTERN "*.lib"
        PATTERN "cmake" EXCLUDE
        PATTERN "pkgconfig" EXCLUDE
        PATTERN "libpng" EXCLUDE
)

# third additional shaders and dependencies
install(DIRECTORY ${EXTERNALS_INSTALL_DIR}/shaders/ DESTINATION shaders)


#------------------------------------------
# Install system collected dependencies
#------------------------------------------

if(UNIX AND COLLECT)
  install(CODE "set(DEPS_INSTALL_DIR \"${EXTERNALS_INSTALL_DIR}/lib\")")
  
  install(CODE [[
      
      message(" ")
      message("Collecting and installing portable build dependencies ...")

      # Feel free to add some missing system dependecies here ;)
      set(REQ_SYS_LIBS 
          "libpython"
          "libOpenColorIO"
          "libicui18n"
          "libicuuc"
          "libjxrglue"
          "libHalf"
          "libcrystalhd"
          "libvpx"
          "libtinyxml"
          "libyaml-cpp"
          "libjpegxr"
          "libicudata"
      )

      string (REPLACE " " ";" REQ_SYS "${REQ_SYS}")

      file(GET_RUNTIME_DEPENDENCIES
          LIBRARIES $<TARGET_FILE:falcor_lib>
          RESOLVED_DEPENDENCIES_VAR _r_deps
          UNRESOLVED_DEPENDENCIES_VAR _u_deps
          CONFLICTING_DEPENDENCIES_PREFIX _c_deps
          #DIRECTORIES ${DEPS_INSTALL_DIR}
      )
      
      foreach(_file ${_r_deps})
          if(${_file} MATCHES "^/usr/*|^/lib/*")
              foreach(REQ_SYS_LIB_NAME ${REQ_SYS_LIBS})
                  if(${_file} MATCHES ".*${REQ_SYS_LIB_NAME}.*")
                      file(INSTALL
                          DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/sys"
                          TYPE SHARED_LIBRARY
                          FOLLOW_SYMLINK_CHAIN
                          FILES "${_file}"
                      )
                  endif()
              endforeach()
          endif()
      endforeach()
      
      foreach(_file ${_u_deps})
          find_library(UNRESOLVED_LIBRARY
            NAMES ${_file}
            PATHS ${DEPS_INSTALL_DIR}
            REQUIRED
            NO_DEFAULT_PATH
          )
          if(NOT UNRESOLVED_LIBRARY)
            message(FATAL_ERROR "${_file} dependency not resolved !!!")
          else()
            file(INSTALL
                DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/3rdparty"
                TYPE SHARED_LIBRARY
                FOLLOW_SYMLINK_CHAIN
                FILES "${UNRESOLVED_LIBRARY}"
            )
            message("Dependency " ${_file} " resolved !")
          endif()
      endforeach()
      
      foreach(_file ${_c_deps_FILENAMES})
          find_library(CONFLICT_LIBRARY
            NAMES ${_file}
            PATHS ${DEPS_INSTALL_DIR}
            REQUIRED
            NO_DEFAULT_PATH
          )
          if(NOT CONFLICT_LIBRARY)
            message(FATAL_ERROR "${_file} conflict not resolved !!!")
          else()
              file(INSTALL
                  DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/3rdparty"
                  TYPE SHARED_LIBRARY
                  FOLLOW_SYMLINK_CHAIN
                  FILES "${CONFLICT_LIBRARY}"
              )
              message("Conflict for " ${_file} " resolved !")
          endif()
      endforeach()
  ]])
endif()

if(WIN32 AND COLLECT)
  install(CODE "set(DEPS_INSTALL_DIR \"${EXTERNALS_INSTALL_DIR}/lib\")")
  
  install(CODE [[
      
      message(" ")
      message("Collecting and installing portable build dependencies ...")

      # Feel free to add some missing system dependecies here ;)
      # set(REQ_SYS_LIBS 
      #     "libpython"
      #     "libOpenColorIO"
      #     "libicui18n"
      #     "libicuuc"
      #     "libjxrglue"
      #     "libHalf"
      #     "libcrystalhd"
      #     "libvpx"
      #     "libtinyxml"
      #     "libyaml-cpp"
      #     "libjpegxr"
      #     "libicudata"
      # )

      string (REPLACE " " ";" REQ_SYS "${REQ_SYS}")

      file(GET_RUNTIME_DEPENDENCIES
          LIBRARIES $<TARGET_FILE:falcor_lib>
          RESOLVED_DEPENDENCIES_VAR _r_deps
          UNRESOLVED_DEPENDENCIES_VAR _u_deps
          CONFLICTING_DEPENDENCIES_PREFIX _c_deps
          #DIRECTORIES ${DEPS_INSTALL_DIR}
      )
      
      # foreach(_file ${_r_deps})
      #     if(${_file} MATCHES "^/usr/*|^/lib/*")
      #         foreach(REQ_SYS_LIB_NAME ${REQ_SYS_LIBS})
      #             if(${_file} MATCHES ".*${REQ_SYS_LIB_NAME}.*")
      #                 file(INSTALL
      #                     DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/sys"
      #                     TYPE SHARED_LIBRARY
      #                     FOLLOW_SYMLINK_CHAIN
      #                     FILES "${_file}"
      #                 )
      #             endif()
      #         endforeach()
      #     endif()
      # endforeach()
      
      foreach(_file ${_u_deps})
          find_library(UNRESOLVED_LIBRARY
            NAMES ${_file}
            PATHS ${DEPS_INSTALL_DIR}
            # REQUIRED
            NO_DEFAULT_PATH
          )
          if(NOT UNRESOLVED_LIBRARY)
            # message(FATAL_ERROR "${_file} dependency not resolved !!!")
            message(WARNING "${_file} dependency not resolved !!!")
          else()
            file(INSTALL
                DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/3rdparty"
                TYPE SHARED_LIBRARY
                FOLLOW_SYMLINK_CHAIN
                FILES "${UNRESOLVED_LIBRARY}"
            )
            message("Dependency " ${_file} " resolved !")
          endif()
      endforeach()
      
      foreach(_file ${_c_deps_FILENAMES})
          find_library(CONFLICT_LIBRARY
            NAMES ${_file}
            PATHS ${DEPS_INSTALL_DIR}
            # REQUIRED
            NO_DEFAULT_PATH
          )
          if(NOT CONFLICT_LIBRARY)
            # message(FATAL_ERROR "${_file} conflict not resolved !!!")
            message(WARNING "${_file} conflict not resolved !!!")
          else()
              file(INSTALL
                  DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/3rdparty"
                  TYPE SHARED_LIBRARY
                  FOLLOW_SYMLINK_CHAIN
                  FILES "${CONFLICT_LIBRARY}"
              )
              message("Conflict for " ${_file} " resolved !")
          endif()
      endforeach()
  ]])
endif()