#[=======================================================================[.rst:

FindSlang
---------

Find Slang include dirs and libraries

Use this module by invoking find_package with the form::

  find_package(Slang
    [version] [EXACT]      # Minimum or EXACT version e.g. 1.5.0
    [REQUIRED]             # Fail with error if Slang is not found
    )

IMPORTED Targets
^^^^^^^^^^^^^^^^

``Slang::slang``
  This module defines IMPORTED target Slang::Slang, if Slang has been found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Slang_FOUND``
  True if the system has the Slang library.
``Slang_VERSION``
  The version of the Slang library which was found.
``Slang_INCLUDE_DIRS``
  Include directories needed to use Slang.
``Slang_RELEASE_LIBRARIES``
  Libraries needed to link to the release version of Slang.
``Slang_RELEASE_LIBRARY_DIRS``
  Slang release library directories.
``Slang_DEBUG_LIBRARIES``
  Libraries needed to link to the debug version of Slang.
``Slang_DEBUG_LIBRARY_DIRS``
  Slang debug library directories.

Deprecated - use [RELEASE|DEBUG] variants:

``Slang_LIBRARIES``
  Libraries needed to link to Slang.
``Slang_LIBRARY_DIRS``
  Slang library directories.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Slang_INCLUDE_DIR``
  The directory containing ``slang.h``.
``Slang_LIBRARY``
  The path to the Slang library. may include target_link_libraries() debug/optimized keywords
``Slang_LIBRARY_RELEASE``
  The path to the Slang release library.
``Slang_LIBRARY_DEBUG``
  The path to the Slang debug library.

Hints
^^^^^

Instead of explicitly setting the cache variables, the following variables
may be provided to tell this module where to look.

``Slang_ROOT``
  Preferred installation prefix.
``SLANG_INCLUDEDIR``
  Preferred include directory e.g. <prefix>/include
``SLANG_LIBRARYDIR``
  Preferred library directory e.g. <prefix>/lib
``SLANG_DEBUG_SUFFIX``
  Suffix of the debug version of slang. Defaults to "_d", OR the empty string for VCPKG_TOOLCHAIN
``SYSTEM_LIBRARY_PATHS``
  Global list of library paths intended to be searched by and find_xxx call
``SLANG_USE_STATIC_LIBS``
  Only search for static slang libraries
``SLANG_USE_EXTERNAL_SOURCES``
  Set to ON if Slang has been built using external sources for LZ4, snappy,
  zlib and zstd. Default is OFF.
``DISABLE_CMAKE_SEARCH_PATHS``
  Disable CMakes default search paths for find_xxx calls in this module

#]=======================================================================]

cmake_minimum_required(VERSION 3.12)
include(GNUInstallDirs)

mark_as_advanced(
  Slang_INCLUDE_DIR
  Slang_LIBRARY
)

set(_FIND_SLANG_ADDITIONAL_OPTIONS "")
if(DISABLE_CMAKE_SEARCH_PATHS)
  set(_FIND_SLANG_ADDITIONAL_OPTIONS NO_DEFAULT_PATH)
endif()

# Set _SLANG_ROOT based on a user provided root var. Xxx_ROOT and ENV{Xxx_ROOT}
# are prioritised over the legacy capitalized XXX_ROOT variables for matching
# CMake 3.12 behaviour
# @todo  deprecate -D and ENV SLANG_ROOT from CMake 3.12
if(Slang_ROOT)
  set(_SLANG_ROOT ${Slang_ROOT})
elseif(DEFINED ENV{Slang_ROOT})
  set(_SLANG_ROOT $ENV{Slang_ROOT})
elseif(SLANG_ROOT)
  set(_SLANG_ROOT ${SLANG_ROOT})
elseif(DEFINED ENV{SLANG_ROOT})
  set(_SLANG_ROOT $ENV{SLANG_ROOT})
endif()

# Additionally try and use pkconfig to find slang
if(USE_PKGCONFIG)
  if(NOT DEFINED PKG_CONFIG_FOUND)
    find_package(PkgConfig)
  endif()
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_Slang QUIET slang)
  endif()
endif()

# ------------------------------------------------------------------------
#  Search for slang include DIR
# ------------------------------------------------------------------------

set(_SLANG_INCLUDE_SEARCH_DIRS "")
list(APPEND _SLANG_INCLUDE_SEARCH_DIRS
  ${SLANG_INCLUDEDIR}
  ${_SLANG_ROOT}
#  ${SYSTEM_LIBRARY_PATHS}
)

# Look for a standard slang header file.
find_path(Slang_INCLUDE_DIR slang/slang.h
  ${_FIND_SLANG_ADDITIONAL_OPTIONS}
  PATHS ${_SLANG_INCLUDE_SEARCH_DIRS}
  PATH_SUFFIXES ${CMAKE_INSTALL_INCLUDEDIR} include
)

if(EXISTS "${Slang_INCLUDE_DIR}/slang/slang-tag-version.h")
  file(STRINGS "${Slang_INCLUDE_DIR}/slang/slang-tag-version.h"
    _slang_version_major_string REGEX "v[0-9]+\\.[0-9]+\\.[0-9]+"
  )

  string(REGEX REPLACE "#define SLANG_TAG_VERSION \"v+([0-9]+)+\\.([0-9]+)+\\.([0-9]+).*$" "\\1"
    _slang_version_major_string "${_slang_version_major_string}"
  )

  string(STRIP "${_slang_version_major_string}" Slang_VERSION_MAJOR)

  file(STRINGS "${Slang_INCLUDE_DIR}/slang/slang-tag-version.h"
     _slang_version_minor_string REGEX "#define SLANG_TAG_VERSION +\"(.*)\""
  )
  string(REGEX REPLACE "#define SLANG_TAG_VERSION \"v+([0-9]+)+\\.([0-9]+)+\\.([0-9]+).*$" "\\2"
    _slang_version_minor_string "${_slang_version_minor_string}"
  )

  string(STRIP "${_slang_version_minor_string}" Slang_VERSION_MINOR)

  file(STRINGS "${Slang_INCLUDE_DIR}/slang/slang-tag-version.h"
     _slang_version_release_string REGEX "#define SLANG_TAG_VERSION +\"(.*)\"" 
  )
  string(REGEX REPLACE "#define SLANG_TAG_VERSION \"v+([0-9]+)+\\.([0-9]+)+\\.([0-9]+).*$" "\\3"
    _slang_version_release_string "${_slang_version_release_string}"
  )
  string(STRIP "${_slang_version_release_string}" Slang_VERSION_RELEASE)

  unset(_slang_version_major_string)
  unset(_slang_version_minor_string)
  unset(_slang_version_release_string)

  set(Slang_VERSION ${Slang_VERSION_MAJOR}.${Slang_VERSION_MINOR}.${Slang_VERSION_RELEASE})
endif()

# ------------------------------------------------------------------------
#  Search for slang lib DIR
# ------------------------------------------------------------------------

set(_SLANG_LIBRARYDIR_SEARCH_DIRS "")
list(APPEND _SLANG_LIBRARYDIR_SEARCH_DIRS
  ${SLANG_LIBRARYDIR}
  ${_SLANG_ROOT}
# ${SYSTEM_LIBRARY_PATHS}
)


# Library suffix handling

set(_SLANG_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})

if(WIN32)
  if(SLANG_USE_STATIC_LIBS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
  endif()
else()
  if(SLANG_USE_STATIC_LIBS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  endif()
endif()

set(Slang_LIB_COMPONENTS "")
# NOTE: Search for debug version first (see vcpkg hack)
list(APPEND SLANG_BUILD_TYPES DEBUG RELEASE)

foreach(BUILD_TYPE ${SLANG_BUILD_TYPES})
  set(_SLANG_LIB_NAME slang)

  set(_SLANG_CMAKE_IGNORE_PATH ${CMAKE_IGNORE_PATH})
  if(VCPKG_TOOLCHAIN)
    # Slang is installed very strangely in VCPKG (debug/release libs have the
    # same name, static build uses external deps, dll doesn't) and slang itself
    # comes with almost zero downstream CMake support for us to detect settings.
    # We should not support external package managers in our own modules like
    # this, but there doesn't seem to be a work around
    if(NOT DEFINED SLANG_DEBUG_SUFFIX)
      set(SLANG_DEBUG_SUFFIX "")
    endif()
    if(BUILD_TYPE STREQUAL RELEASE)
      if(EXISTS ${Slang_LIBRARY_DEBUG})
        get_filename_component(_SLANG_DEBUG_DIR ${Slang_LIBRARY_DEBUG} DIRECTORY)
        list(APPEND CMAKE_IGNORE_PATH ${_SLANG_DEBUG_DIR})
      endif()
    endif()
  endif()

  if(BUILD_TYPE STREQUAL DEBUG)
    if(NOT DEFINED SLANG_DEBUG_SUFFIX)
      set(SLANG_DEBUG_SUFFIX _d)
    endif()
    set(_SLANG_LIB_NAME "${_SLANG_LIB_NAME}${SLANG_DEBUG_SUFFIX}")
  endif()

  find_library(Slang_LIBRARY_${BUILD_TYPE} ${_SLANG_LIB_NAME}
    ${_FIND_SLANG_ADDITIONAL_OPTIONS}
    PATHS ${_SLANG_LIBRARYDIR_SEARCH_DIRS}
    PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR} lib64 lib
  )

  list(APPEND Slang_LIB_COMPONENTS ${Slang_LIBRARY_${BUILD_TYPE}})
  set(CMAKE_IGNORE_PATH ${_SLANG_CMAKE_IGNORE_PATH})
endforeach()

# Reset library suffix

set(CMAKE_FIND_LIBRARY_SUFFIXES ${_SLANG_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
unset(_SLANG_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES)

if(Slang_LIBRARY_DEBUG AND Slang_LIBRARY_RELEASE)
  # if the generator is multi-config or if CMAKE_BUILD_TYPE is set for
  # single-config generators, set optimized and debug libraries
  get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(_isMultiConfig OR CMAKE_BUILD_TYPE)
    set(Slang_LIBRARY optimized ${Slang_LIBRARY_RELEASE} debug ${Slang_LIBRARY_DEBUG})
  else()
    # For single-config generators where CMAKE_BUILD_TYPE has no value,
    # just use the release libraries
    set(Slang_LIBRARY ${Slang_LIBRARY_RELEASE})
  endif()
  # FIXME: This probably should be set for both cases
  set(Slang_LIBRARIES optimized ${Slang_LIBRARY_RELEASE} debug ${Slang_LIBRARY_DEBUG})
endif()

# if only the release version was found, set the debug variable also to the release version
if(Slang_LIBRARY_RELEASE AND NOT Slang_LIBRARY_DEBUG)
  set(Slang_LIBRARY_DEBUG ${Slang_LIBRARY_RELEASE})
  set(Slang_LIBRARY       ${Slang_LIBRARY_RELEASE})
  set(Slang_LIBRARIES     ${Slang_LIBRARY_RELEASE})
endif()

# if only the debug version was found, set the release variable also to the debug version
if(Slang_LIBRARY_DEBUG AND NOT Slang_LIBRARY_RELEASE)
  set(Slang_LIBRARY_RELEASE ${Slang_LIBRARY_DEBUG})
  set(Slang_LIBRARY         ${Slang_LIBRARY_DEBUG})
  set(Slang_LIBRARIES       ${Slang_LIBRARY_DEBUG})
endif()

# If the debug & release library ends up being the same, omit the keywords
if("${Slang_LIBRARY_RELEASE}" STREQUAL "${Slang_LIBRARY_DEBUG}")
  set(Slang_LIBRARY   ${Slang_LIBRARY_RELEASE} )
  set(Slang_LIBRARIES ${Slang_LIBRARY_RELEASE} )
endif()

if(Slang_LIBRARY)
  set(Slang_FOUND TRUE)
else()
  set(Slang_FOUND FALSE)
endif()

# ------------------------------------------------------------------------
#  Cache and set Slang_FOUND
# ------------------------------------------------------------------------

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Slang
  FOUND_VAR Slang_FOUND
  REQUIRED_VARS
    Slang_LIBRARY
    Slang_INCLUDE_DIR
  VERSION_VAR Slang_VERSION
)

if(NOT Slang_FOUND)
  if(Slang_FIND_REQUIRED)
    message(FATAL_ERROR "Unable to find Slang")
  endif()
  return()
endif()

# Partition release/debug lib vars

set(Slang_RELEASE_LIBRARIES ${Slang_LIBRARY_RELEASE})
get_filename_component(Slang_RELEASE_LIBRARY_DIRS ${Slang_RELEASE_LIBRARIES} DIRECTORY)

set(Slang_DEBUG_LIBRARIES ${Slang_LIBRARY_DEBUG})
get_filename_component(Slang_DEBUG_LIBRARY_DIRS ${Slang_DEBUG_LIBRARIES} DIRECTORY)

set(Slang_LIBRARIES ${Slang_RELEASE_LIBRARIES})
set(Slang_LIBRARY_DIRS ${Slang_RELEASE_LIBRARY_DIRS})

set(Slang_INCLUDE_DIRS ${Slang_INCLUDE_DIR})
set(Slang_INCLUDE_DIRS ${Slang_INCLUDE_DIR})

# Configure lib type. If XXX_USE_STATIC_LIBS, we always assume a static
# lib is in use. If win32, we can't mark the import .libs as shared, so
# these are always marked as UNKNOWN. Otherwise, infer from extension.
set(SLANG_LIB_TYPE UNKNOWN)
if(SLANG_USE_STATIC_LIBS)
  set(SLANG_LIB_TYPE STATIC)
elseif(UNIX)
  get_filename_component(_SLANG_EXT ${Slang_LIBRARY_RELEASE} EXT)
  if(_SLANG_EXT STREQUAL ".a")
    set(SLANG_LIB_TYPE STATIC)
  elseif(_SLANG_EXT STREQUAL ".so" OR
         _SLANG_EXT STREQUAL ".dylib")
    set(SLANG_LIB_TYPE SHARED)
  endif()
endif()

get_filename_component(Slang_LIBRARY_DIRS ${Slang_LIBRARY_RELEASE} DIRECTORY)

if(NOT TARGET Slang::slang)
  add_library(Slang::slang ${SLANG_LIB_TYPE} IMPORTED)
  set_target_properties(Slang::slang PROPERTIES
    INTERFACE_COMPILE_OPTIONS "${PC_Slang_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${Slang_INCLUDE_DIRS}")

  # Standard location
  set_target_properties(Slang::slang PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    IMPORTED_LOCATION "${Slang_LIBRARY}")

  # Release location
  if(EXISTS "${Slang_LIBRARY_RELEASE}")
    set_property(TARGET Slang::slang APPEND PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE)
    set_target_properties(Slang::slang PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
      IMPORTED_LOCATION_RELEASE "${Slang_LIBRARY_RELEASE}")
  endif()

  # Debug location
  if(EXISTS "${Slang_LIBRARY_DEBUG}")
    set_property(TARGET Slang::slang APPEND PROPERTY
      IMPORTED_CONFIGURATIONS DEBUG)
    set_target_properties(Slang::slang PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
      IMPORTED_LOCATION_DEBUG "${Slang_LIBRARY_DEBUG}")
  endif()

  # Slang may optionally be compiled with external sources for
  # lz4, snappy and zlib . Add them as interface libs if requested
  # (there doesn't seem to be a way to figure this out automatically).
  # We assume they live along side slang
  if(SLANG_USE_EXTERNAL_SOURCES)
    set_target_properties(Slang::slang PROPERTIES
      INTERFACE_LINK_DIRECTORIES
         "\$<\$<CONFIG:Release>:${Slang_RELEASE_LIBRARY_DIRS}>;\$<\$<CONFIG:Debug>:${Slang_DEBUG_LIBRARY_DIRS}>")
    target_link_libraries(Slang::slang INTERFACE
      $<$<CONFIG:Release>:lz4;snappy;zlib>
      $<$<CONFIG:Debug>:lz4d;snappyd;zlibd>)

    if(SLANG_USE_STATIC_LIBS)
      target_link_libraries(Slang::slang INTERFACE
        $<$<CONFIG:Release>:zstd_static>
        $<$<CONFIG:Debug>:zstd_staticd>)
    else()
      target_link_libraries(Slang::slang INTERFACE
        $<$<CONFIG:Release>:zstd>
        $<$<CONFIG:Debug>:zstdd>)
    endif()
  endif()
endif()

