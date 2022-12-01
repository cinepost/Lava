#[=======================================================================[.rst:

FindOIDN
---------

Find OIDN include dirs and libraries

Use this module by invoking find_package with the form::

  find_package(OIDN
    [version] [EXACT]      # Minimum or EXACT version e.g. 1.5.0
    [REQUIRED]             # Fail with error if OIDN is not found
    )

IMPORTED Targets
^^^^^^^^^^^^^^^^

``OIDN::oidn``
  This module defines IMPORTED target OIDN::OIDN, if OIDN has been found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``OIDN_FOUND``
  True if the system has the OIDN library.
``OIDN_VERSION``
  The version of the OIDN library which was found.
``OIDN_INCLUDE_DIRS``
  Include directories needed to use OIDN.
``OIDN_RELEASE_LIBRARIES``
  Libraries needed to link to the release version of OIDN.
``OIDN_RELEASE_LIBRARY_DIRS``
  OIDN release library directories.
``OIDN_DEBUG_LIBRARIES``
  Libraries needed to link to the debug version of OIDN.
``OIDN_DEBUG_LIBRARY_DIRS``
  OIDN debug library directories.

Deprecated - use [RELEASE|DEBUG] variants:

``OIDN_LIBRARIES``
  Libraries needed to link to OIDN.
``OIDN_LIBRARY_DIRS``
  OIDN library directories.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``OIDN_INCLUDE_DIR``
  The directory containing ``oidn.h``.
``OIDN_LIBRARY``
  The path to the OIDN library. may include target_link_libraries() debug/optimized keywords
``OIDN_LIBRARY_RELEASE``
  The path to the OIDN release library.
``OIDN_LIBRARY_DEBUG``
  The path to the OIDN debug library.

Hints
^^^^^

Instead of explicitly setting the cache variables, the following variables
may be provided to tell this module where to look.

``OIDN_ROOT``
  Preferred installation prefix.
``OIDN_INCLUDEDIR``
  Preferred include directory e.g. <prefix>/include
``OIDN_LIBRARYDIR``
  Preferred library directory e.g. <prefix>/lib
``OIDN_DEBUG_SUFFIX``
  Suffix of the debug version of oidn. Defaults to "_d", OR the empty string for VCPKG_TOOLCHAIN
``SYSTEM_LIBRARY_PATHS``
  Global list of library paths intended to be searched by and find_xxx call
``OIDN_USE_STATIC_LIBS``
  Only search for static oidn libraries
``OIDN_USE_EXTERNAL_SOURCES``
  Set to ON if OIDN has been built using external sources for LZ4, snappy,
  zlib and zstd. Default is OFF.
``DISABLE_CMAKE_SEARCH_PATHS``
  Disable CMakes default search paths for find_xxx calls in this module

#]=======================================================================]

cmake_minimum_required(VERSION 3.12)
include(GNUInstallDirs)

mark_as_advanced(
  OIDN_INCLUDE_DIR
  OIDN_LIBRARY
)

set(_FIND_OIDN_ADDITIONAL_OPTIONS "")
if(DISABLE_CMAKE_SEARCH_PATHS)
  set(_FIND_OIDN_ADDITIONAL_OPTIONS NO_DEFAULT_PATH)
endif()

# Set _OIDN_ROOT based on a user provided root var. Xxx_ROOT and ENV{Xxx_ROOT}
# are prioritised over the legacy capitalized XXX_ROOT variables for matching
# CMake 3.12 behaviour
# @todo  deprecate -D and ENV OIDN_ROOT from CMake 3.12
if(OIDN_ROOT)
  set(_OIDN_ROOT ${OIDN_ROOT})
elseif(DEFINED ENV{OIDN_ROOT})
  set(_OIDN_ROOT $ENV{OIDN_ROOT})
elseif(OIDN_ROOT)
  set(_OIDN_ROOT ${OIDN_ROOT})
elseif(DEFINED ENV{OIDN_ROOT})
  set(_OIDN_ROOT $ENV{OIDN_ROOT})
endif()

# Additionally try and use pkconfig to find oidn
if(USE_PKGCONFIG)
  if(NOT DEFINED PKG_CONFIG_FOUND)
    find_package(PkgConfig)
  endif()
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_OIDN QUIET oidn)
  endif()
endif()

# ------------------------------------------------------------------------
#  Search for oidn include DIR
# ------------------------------------------------------------------------

set(_OIDN_INCLUDE_SEARCH_DIRS "")
list(APPEND _OIDN_INCLUDE_SEARCH_DIRS
  ${OIDN_INCLUDEDIR}
  ${_OIDN_ROOT}
#  ${SYSTEM_LIBRARY_PATHS}
)

# Look for a standard oidn header file.
find_path(OIDN_INCLUDE_DIR OpenImageDenoise/oidn.h
  ${_FIND_OIDN_ADDITIONAL_OPTIONS}
  PATHS ${_OIDN_INCLUDE_SEARCH_DIRS}
  PATH_SUFFIXES ${CMAKE_INSTALL_INCLUDEDIR} include
)

if(EXISTS "${OIDN_INCLUDE_DIR}/OpenImageDenoise/config.h")
  file( STRINGS ${OIDN_INCLUDE_DIR}/OpenImageDenoise/config.h 
    VERSION_MAJOR_STR REGEX "#define[ ]+OIDN_VERSION_MAJOR[ ]+[0-9]+"
  )

  file( STRINGS ${OIDN_INCLUDE_DIR}/OpenImageDenoise/config.h
    VERSION_MINOR_STR REGEX "#define[ ]+OIDN_VERSION_MINOR[ ]+[0-9]+"
  )

  file( STRINGS ${OIDN_INCLUDE_DIR}/OpenImageDenoise/config.h
    VERSION_PATCH_STR REGEX "#define[ ]+OIDN_VERSION_PATCH[ ]+[0-9]+"
  )

  if (VERSION_MAJOR_STR)
    string(REGEX MATCH "[0-9]+" VERSION_MAJOR_STR ${VERSION_MAJOR_STR})
  endif()

  if (VERSION_MINOR_STR)
    string(REGEX MATCH "[0-9]+" VERSION_MINOR_STR ${VERSION_MINOR_STR})
  endif()

  if (VERSION_PATCH_STR)  
    string(REGEX MATCH "[0-9]+" VERSION_PATCH_STR ${VERSION_PATCH_STR})
  endif()

  if (VERSION_MAJOR_STR AND VERSION_MINOR_STR AND VERSION_PATCH_STR) 
    set(OIDN_VERSION ${VERSION_MAJOR_STR}.${VERSION_MINOR_STR}.${VERSION_PATCH_STR})
  endif()
endif()

# ------------------------------------------------------------------------
#  Search for oidn lib DIR
# ------------------------------------------------------------------------

set(_OIDN_LIBRARYDIR_SEARCH_DIRS "")
list(APPEND _OIDN_LIBRARYDIR_SEARCH_DIRS
  ${OIDN_LIBRARYDIR}
  ${_OIDN_ROOT}
# ${SYSTEM_LIBRARY_PATHS}
)


# Library suffix handling

set(_OIDN_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})

if(WIN32)
  if(OIDN_USE_STATIC_LIBS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
  endif()
else()
  if(OIDN_USE_STATIC_LIBS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  endif()
endif()

set(OIDN_LIB_COMPONENTS "")
# NOTE: Search for debug version first (see vcpkg hack)
list(APPEND OIDN_BUILD_TYPES DEBUG RELEASE)

foreach(BUILD_TYPE ${OIDN_BUILD_TYPES})
  set(_OIDN_LIB_NAME OpenImageDenoise)
  set(_DNNL_LIB_NAME dnnl)

  set(_OIDN_CMAKE_IGNORE_PATH ${CMAKE_IGNORE_PATH})
  if(VCPKG_TOOLCHAIN)
    # OIDN is installed very strangely in VCPKG (debug/release libs have the
    # same name, static build uses external deps, dll doesn't) and oidn itself
    # comes with almost zero downstream CMake support for us to detect settings.
    # We should not support external package managers in our own modules like
    # this, but there doesn't seem to be a work around
    if(NOT DEFINED OIDN_DEBUG_SUFFIX)
      set(OIDN_DEBUG_SUFFIX "")
    endif()
    if(BUILD_TYPE STREQUAL RELEASE)
      if(EXISTS ${OIDN_LIBRARY_DEBUG})
        get_filename_component(_OIDN_DEBUG_DIR ${OIDN_LIBRARY_DEBUG} DIRECTORY)
        list(APPEND CMAKE_IGNORE_PATH ${_OIDN_DEBUG_DIR})
      endif()
    endif()
  endif()

  if(BUILD_TYPE STREQUAL DEBUG)
    if(NOT DEFINED OIDN_DEBUG_SUFFIX)
      set(OIDN_DEBUG_SUFFIX _d)
    endif()
    set(_OIDN_LIB_NAME "${_OIDN_LIB_NAME}${OIDN_DEBUG_SUFFIX}")
    set(_DNNL_LIB_NAME "${_DNNL_LIB_NAME}${OIDN_DEBUG_SUFFIX}")
  endif()

  find_library(OIDN_LIBRARY_${BUILD_TYPE} ${_OIDN_LIB_NAME}
    ${_FIND_OIDN_ADDITIONAL_OPTIONS}
    PATHS ${_OIDN_LIBRARYDIR_SEARCH_DIRS}
    PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR} lib64 lib
  )

  find_library(DNNL_LIBRARY_${BUILD_TYPE} ${_DNNL_LIB_NAME}
    ${_FIND_OIDN_ADDITIONAL_OPTIONS}
    PATHS ${_OIDN_LIBRARYDIR_SEARCH_DIRS}
    PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR} lib64 lib
  )

  list(APPEND OIDN_LIB_COMPONENTS ${OIDN_LIBRARY_${BUILD_TYPE}})
  list(APPEND OIDN_LIB_COMPONENTS ${DNNL_LIBRARY_${BUILD_TYPE}})

  set(CMAKE_IGNORE_PATH ${_OIDN_CMAKE_IGNORE_PATH})
endforeach()

# Reset library suffix

set(CMAKE_FIND_LIBRARY_SUFFIXES ${_OIDN_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
unset(_OIDN_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES)

if(OIDN_LIBRARY_DEBUG AND OIDN_LIBRARY_RELEASE)
  # if the generator is multi-config or if CMAKE_BUILD_TYPE is set for
  # single-config generators, set optimized and debug libraries
  get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(_isMultiConfig OR CMAKE_BUILD_TYPE)
    set(OIDN_LIBRARY optimized ${OIDN_LIBRARY_RELEASE} debug ${OIDN_LIBRARY_DEBUG})
    set(DNNL_LIBRARY optimized ${DNNL_LIBRARY_RELEASE} debug ${DNNL_LIBRARY_DEBUG})
  else()
    # For single-config generators where CMAKE_BUILD_TYPE has no value,
    # just use the release libraries
    set(OIDN_LIBRARY ${OIDN_LIBRARY_RELEASE})
    set(DNNL_LIBRARY ${DNNL_LIBRARY_RELEASE})
  endif()
  # FIXME: This probably should be set for both cases
  set(OIDN_LIBRARIES optimized ${OIDN_LIBRARY_RELEASE} ${DNNL_LIBRARY_RELEASE} debug ${OIDN_LIBRARY_DEBUG} ${DNNL_LIBRARY_DEBUG})
endif()

# if only the release version was found, set the debug variable also to the release version
if(OIDN_LIBRARY_RELEASE AND NOT OIDN_LIBRARY_DEBUG)
  set(OIDN_LIBRARY_DEBUG ${OIDN_LIBRARY_RELEASE})
  set(OIDN_LIBRARY       ${OIDN_LIBRARY_RELEASE})
  set(DNNL_LIBRARY       ${DNNL_LIBRARY_RELEASE})
  set(OIDN_LIBRARIES     ${OIDN_LIBRARY_RELEASE})
endif()

# if only the debug version was found, set the release variable also to the debug version
if(OIDN_LIBRARY_DEBUG AND NOT OIDN_LIBRARY_RELEASE)
  set(OIDN_LIBRARY_RELEASE ${OIDN_LIBRARY_DEBUG})
  set(OIDN_LIBRARY         ${OIDN_LIBRARY_DEBUG})
  set(DNNL_LIBRARY         ${DNNL_LIBRARY_DEBUG})
  set(OIDN_LIBRARIES       ${OIDN_LIBRARY_DEBUG})
endif()

# If the debug & release library ends up being the same, omit the keywords
if("${OIDN_LIBRARY_RELEASE}" STREQUAL "${OIDN_LIBRARY_DEBUG}")
  set(OIDN_LIBRARY   ${OIDN_LIBRARY_RELEASE} )
  set(DNNL_LIBRARY   ${DNNL_LIBRARY_RELEASE})
  set(OIDN_LIBRARIES ${OIDN_LIBRARY_RELEASE} )
endif()

if(OIDN_LIBRARY)
  set(OIDN_FOUND TRUE)
else()
  set(OIDN_FOUND FALSE)
endif()

# ------------------------------------------------------------------------
#  Cache and set OIDN_FOUND
# ------------------------------------------------------------------------

include(FindPackageHandleStandardArgs)

if(OIDN_USE_STATIC_LIBS)
  find_package_handle_standard_args(OIDN
    FOUND_VAR OIDN_FOUND
    REQUIRED_VARS
      OIDN_LIBRARY
      DNNL_LIBRARY
      OIDN_INCLUDE_DIR
    VERSION_VAR OIDN_VERSION
  )
else()
  find_package_handle_standard_args(OIDN
    FOUND_VAR OIDN_FOUND
    REQUIRED_VARS
      OIDN_LIBRARY
      OIDN_INCLUDE_DIR
    VERSION_VAR OIDN_VERSION
  )
endif()

if(NOT OIDN_FOUND)
  if(OIDN_FIND_REQUIRED)
    message(FATAL_ERROR "Unable to find OIDN")
  endif()
  return()
endif()

# Partition release/debug lib vars

if(OIDN_USE_STATIC_LIBS)
  set(OIDN_RELEASE_LIBRARIES ${OIDN_LIBRARY_RELEASE} ${DNNL_LIBRARY_RELEASE})
else()
  set(OIDN_RELEASE_LIBRARIES ${OIDN_LIBRARY_RELEASE})
endif()
get_filename_component(OIDN_RELEASE_LIBRARY_DIRS ${OIDN_LIBRARY_RELEASE} DIRECTORY)

if(OIDN_USE_STATIC_LIBS)
  set(OIDN_DEBUG_LIBRARIES ${OIDN_LIBRARY_DEBUG} ${DNNL_LIBRARY_DEBUG})
else()
  set(OIDN_DEBUG_LIBRARIES ${OIDN_LIBRARY_DEBUG})
endif()
get_filename_component(OIDN_DEBUG_LIBRARY_DIRS ${OIDN_LIBRARY_DEBUG} DIRECTORY)

set(OIDN_LIBRARIES ${OIDN_RELEASE_LIBRARIES})
set(OIDN_LIBRARY_DIRS ${OIDN_RELEASE_LIBRARY_DIRS})

set(OIDN_INCLUDE_DIRS ${OIDN_INCLUDE_DIR})
set(OIDN_INCLUDE_DIRS ${OIDN_INCLUDE_DIR})

# Configure lib type. If XXX_USE_STATIC_LIBS, we always assume a static
# lib is in use. If win32, we can't mark the import .libs as shared, so
# these are always marked as UNKNOWN. Otherwise, infer from extension.
set(OIDN_LIB_TYPE UNKNOWN)
if(OIDN_USE_STATIC_LIBS)
  set(OIDN_LIB_TYPE STATIC)
elseif(UNIX)
  get_filename_component(_OIDN_EXT ${OIDN_LIBRARY_RELEASE} EXT)
  if(_OIDN_EXT STREQUAL ".a")
    set(OIDN_LIB_TYPE STATIC)
  elseif(_OIDN_EXT STREQUAL ".so" OR
         _OIDN_EXT STREQUAL ".dylib")
    set(OIDN_LIB_TYPE SHARED)
  endif()
endif()

get_filename_component(OIDN_LIBRARY_DIRS ${OIDN_LIBRARY_RELEASE} DIRECTORY)

if(NOT TARGET OIDN::oidn)
  add_library(OIDN::oidn ${OIDN_LIB_TYPE} IMPORTED)
  set_target_properties(OIDN::oidn PROPERTIES
    INTERFACE_COMPILE_OPTIONS "${PC_OIDN_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${OIDN_INCLUDE_DIRS}")

  # Standard location
  set_target_properties(OIDN::oidn PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    IMPORTED_LOCATION "${OIDN_LIBRARY}")

  # Release location
  if(EXISTS "${OIDN_LIBRARY_RELEASE}")
    set_property(TARGET OIDN::oidn APPEND PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE)
    set_target_properties(OIDN::oidn PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
      IMPORTED_LOCATION_RELEASE "${OIDN_LIBRARY_RELEASE}")
  endif()

  # Debug location
  if(EXISTS "${OIDN_LIBRARY_DEBUG}")
    set_property(TARGET OIDN::oidn APPEND PROPERTY
      IMPORTED_CONFIGURATIONS DEBUG)
    set_target_properties(OIDN::oidn PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
      IMPORTED_LOCATION_DEBUG "${OIDN_LIBRARY_DEBUG}")
  endif()

  # OIDN may optionally be compiled with external sources for
  # lz4, snappy and zlib . Add them as interface libs if requested
  # (there doesn't seem to be a way to figure this out automatically).
  # We assume they live along side oidn
  if(OIDN_USE_EXTERNAL_SOURCES)
    set_target_properties(OIDN::oidn PROPERTIES
      INTERFACE_LINK_DIRECTORIES
         "\$<\$<CONFIG:Release>:${OIDN_RELEASE_LIBRARY_DIRS}>;\$<\$<CONFIG:Debug>:${OIDN_DEBUG_LIBRARY_DIRS}>")
    target_link_libraries(OIDN::oidn INTERFACE
      $<$<CONFIG:Release>:lz4;snappy;zlib>
      $<$<CONFIG:Debug>:lz4d;snappyd;zlibd>)

    if(OIDN_USE_STATIC_LIBS)
      target_link_libraries(OIDN::oidn INTERFACE
        $<$<CONFIG:Release>:zstd_static>
        $<$<CONFIG:Debug>:zstd_staticd>)
    else()
      target_link_libraries(OIDN::oidn INTERFACE
        $<$<CONFIG:Release>:zstd>
        $<$<CONFIG:Debug>:zstdd>)
    endif()
  endif()
endif()

