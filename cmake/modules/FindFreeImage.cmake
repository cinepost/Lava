# Find the FreeImage library.
#
# Once done this will define
#  FreeImage_FOUND - System has FreeImage
#  FreeImage_INCLUDE_DIRS - The FreeImage include directories
#  FreeImage_LIBRARIES - The libraries needed to use FreeImage
#  FreeImage_DEFINITIONS - Compiler switches required for using FreeImage
#
# It also creates the IMPORTED target: FreeImage::FreeImage

unset( _FreeImage_INCLUDE_DIR CACHE)
unset( _FreeImage_LIBRARY_RELEASE CACHE)
unset( _FreeImage_LIBRARY_DEBUG CACHE)
unset( FreeImage_FOUND CACHE)

find_path(_FreeImage_INCLUDE_DIR 
	NAMES FreeImage.h
	PATHS ${FreeImage_ROOT}/include
	NO_DEFAULT_PATH
)

find_library(_FreeImage_LIBRARY_RELEASE 
	NAMES FreeImage freeimage
	PATHS ${FreeImage_ROOT}/../release/lib
	NO_DEFAULT_PATH
)

find_library(_FreeImage_LIBRARY_DEBUG
  NAMES FreeImage freeimage
  PATHS ${FreeImage_ROOT}/../debug/lib
  NO_DEFAULT_PATH
)

if(EXISTS "${_FreeImage_INCLUDE_DIR}/FreeImage.h")
	file( STRINGS ${_FreeImage_INCLUDE_DIR}/FreeImage.h
    FreeImage_VERSION_MAJOR REGEX "#define[ ]+FREEIMAGE_MAJOR_VERSION[ ]+[0-9]+"
  )

  file( STRINGS ${_FreeImage_INCLUDE_DIR}/FreeImage.h
    FreeImage_VERSION_MINOR REGEX "#define[ ]+FREEIMAGE_MINOR_VERSION[ ]+[0-9]+"
  )

  file( STRINGS ${_FreeImage_INCLUDE_DIR}/FreeImage.h
    FreeImage_VERSION_RELEASE REGEX "#define[ ]+FREEIMAGE_RELEASE_SERIAL[ ]+[0-9]+"
  )

  if (FreeImage_VERSION_MAJOR)
    string(REGEX MATCH "[0-9]+" FreeImage_VERSION_MAJOR ${FreeImage_VERSION_MAJOR})
  endif()

  if (FreeImage_VERSION_MINOR)
    string(REGEX MATCH "[0-9]+" FreeImage_VERSION_MINOR ${FreeImage_VERSION_MINOR})
  endif()

  if (FreeImage_VERSION_RELEASE)  
    string(REGEX MATCH "[0-9]+" FreeImage_VERSION_RELEASE ${FreeImage_VERSION_RELEASE})
  endif()

  set(FreeImage_VERSION ${FreeImage_VERSION_MAJOR}.${FreeImage_VERSION_MINOR}.${FreeImage_VERSION_RELEASE})
else()
	set(FreeImage_VERSION "")
endif()

message("FreeImage_VERSION ${FreeImage_VERSION}")


include(FindPackageHandleStandardArgs)

#find_package_handle_standard_args(FreeImage  DEFAULT_MSG
#                                  _FreeImage_LIBRARY_RELEASE
#                                  _FreeImage_INCLUDE_DIR
#  REQUIRED_VARS _FreeImage_INCLUDE_DIR
#  VERSION_VAR FreeImage_VERSION)

find_package_handle_standard_args(FreeImage
  REQUIRED_VARS _FreeImage_INCLUDE_DIR
  VERSION_VAR FreeImage_VERSION
)


mark_as_advanced(_FreeImage_INCLUDE_DIR
                 _FreeImage_LIBRARY_DEBUG
                 _FreeImage_LIBRARY_RELEASE)

set(FreeImage_LIBRARY "")
if (_FreeImage_LIBRARY_DEBUG)
    list(APPEND FreeImage_LIBRARY debug ${_FreeImage_LIBRARY_DEBUG})
endif()
if (_FreeImage_LIBRARY_RELEASE)
    list(APPEND FreeImage_LIBRARY optimized ${_FreeImage_LIBRARY_RELEASE})
endif()

set(FreeImage_INCLUDE_DIRS ${_FreeImage_INCLUDE_DIR})
set(FreeImage_LIBRARIES ${FreeImage_LIBRARY})

if(FreeImage_FOUND)
  if (NOT TARGET FreeImage::FreeImage)
    add_library(FreeImage::FreeImage UNKNOWN IMPORTED)
  endif()
  if (_FreeImage_LIBRARY_RELEASE)
    set_property(TARGET FreeImage::FreeImage APPEND PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE
    )
    set_target_properties(FreeImage::FreeImage PROPERTIES
      IMPORTED_LOCATION_RELEASE "${_FreeImage_LIBRARY_RELEASE}"
    )
  endif()
  if (_FreeImage_LIBRARY_DEBUG)
    set_property(TARGET FreeImage::FreeImage APPEND PROPERTY
      IMPORTED_CONFIGURATIONS DEBUG
    )
    set_target_properties(FreeImage::FreeImage PROPERTIES
      IMPORTED_LOCATION_DEBUG "${_FreeImage_LIBRARY_DEBUG}"
    )
  endif()
  set_target_properties(FreeImage::FreeImage PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_FreeImage_INCLUDE_DIR}"
  )
endif()
