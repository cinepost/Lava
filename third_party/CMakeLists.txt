include(ExternalProject)

ExternalProject_Add( ThirdParty_zlib
  PREFIX zlib
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/zlib"
  CMAKE_CACHE_ARGS
    -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/zlib
)

#######################
# PyBind11
#######################

ExternalProject_Add ( ThirdParty_pybind11
  PREFIX pybind11
  DOWNLOAD_COMMAND "" # Disable download step
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pybind11" # Source dir for configuration
  #INSTALL_DIR ${IMATH_BUILD_PATH}
  CMAKE_CACHE_ARGS
    -DPYBIND11_INSTALL:BOOL=ON
    -DPYBIND11_MASTER_PROJECT:BOOL=ON
    -DBUILD_TESTING:BOOL=OFF 
    -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/pybind11
)

#######################
# Imath
#######################

ExternalProject_Add ( ThirdParty_Imath
    PREFIX Imath
    DOWNLOAD_COMMAND "" # Disable download step
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Imath" # Source dir for configuration
    #INSTALL_DIR ${IMATH_BUILD_PATH}
    CMAKE_CACHE_ARGS 
      -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/imath
)

#######################
# OpenEXR
#######################

ExternalProject_Add ( ThirdParty_OpenEXR
    PREFIX OpenEXR
    DOWNLOAD_COMMAND "" # Disable download step
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OpenEXR" # Source dir for configuration
    #INSTALL_DIR ${IMATH_BUILD_PATH}
    CMAKE_CACHE_ARGS 
      -DOPENEXR_IS_SUBPROJECT:BOOL=ON
      -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/openexr
)

#######################
# OpenImageIO
#######################

ExternalProject_Add ( ThirdParty_OpenImageIO
    PREFIX OpenImageIO
    DOWNLOAD_COMMAND "" # Disable download step
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OpenImageIO" # Source dir for configuration
    #INSTALL_DIR ${IMATH_BUILD_PATH}
    CMAKE_CACHE_ARGS 
      -Dpybind11_ROOT:PATH=${CMAKE_CURRENT_BINARY_DIR}/pybind11
      -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/openimageio
)

#######################
# FreeImage
#######################

add_custom_target(
  ThirdParty_FreeImage
  COMMAND make
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/FreeImage"
)

set( FREEIMAGE_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/FreeImage/Dist" PARENT_SCOPE )
set( FREEIMAGE_LIBRARIES "${CMAKE_CURRENT_BINARY_DIR}/FreeImage/Dist/libfreeimage-3.18.0.so" PARENT_SCOPE )

# Deps

add_dependencies( ThirdParty_OpenEXR ThirdParty_Imath )
add_dependencies( ThirdParty_OpenImageIO ThirdParty_pybind11 )
add_dependencies( ThirdParty_OpenImageIO ThirdParty_OpenEXR )