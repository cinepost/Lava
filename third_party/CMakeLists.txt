cmake_minimum_required(VERSION 3.17)

project("ThirdParty_External")

include(ExternalProject)

message("Running third party dependencies build...")

set(NCPU 2)

set( CMAKE_MODULE_PATH 
  ${PROJECT_SOURCE_DIR}/../cmake/modules
)

set(EXTRA_CMAKE_ARGS)
set(LAVA_3RD_LIB_LINK_WITH_STATIC OFF)

#set(EXTERNALS_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/external)
set(EXTERNALS_BUILD_DIR ${PROJECT_SOURCE_DIR}/../build_external)
set(EXTERNALS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)

find_program(MAKE_EXECUTABLE
  NAMES gmake mingw32-make make
  NAMES_PER_DIR
  DOC "GNU Make"
)

find_program(CHMOD_EXECUTABLE
  NAMES chmod
  NAMES_PER_DIR
)

find_package(Git)
if(Git_FOUND)
  message("Git found: ${GIT_EXECUTABLE}")
endif()

#######################
# TBB
#######################

set(EXTERNAL_TBB_VERSION 2020.2)
set(TBB_ROOT ${EXTERNALS_BUILD_DIR})
find_package( TBB ${EXTERNAL_TBB_VERSION} EXACT)

if( TBB_FOUND )

  message("TBB version ${Tbb_VERSION} found ...")
  add_custom_target( Tbb COMMAND echo "TBB build exists, skipping rebuild ...")
else()

  message("TBB not found! Running external build ...")
  set(TBB_PREFIX tbb)

  ExternalProject_Add( Tbb
    PREFIX ${EXTERNALS_SOURCE_DIR}/${TBB_PREFIX}
    GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
    GIT_TAG v${EXTERNAL_TBB_VERSION}
    UPDATE_DISCONNECTED true
    BUILD_IN_SOURCE 1
    
    CONFIGURE_COMMAND ""
    
    BUILD_COMMAND ${MAKE_EXECUTABLE} -j${NCPU} tbb_build_prefix=${TBB_PREFIX}
    
    #INSTALL_COMMAND ${CMAKE_COMMAND}
    #  -E copy_directory <SOURCE_DIR>/include/tbb ${EXTERNALS_BUILD_DIR}/include/tbb

    INSTALL_DIR ${EXTERNALS_BUILD_DIR}
    
    INSTALL_COMMAND ${CMAKE_COMMAND}
      -D TBB_LIBS_DIR=<SOURCE_DIR>/build/${TBB_PREFIX}_release
      -D TBB_INCL_DIR=<SOURCE_DIR>/include/tbb
      -D DEST_DIR=${EXTERNALS_BUILD_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tbb_install_files.cmake
  )

endif()

#######################
# Blosc2
#######################

set(Blosc_ROOT ${EXTERNALS_BUILD_DIR})
find_package(Blosc)

if( Blosc_FOUND )

  message("Blosc version ${Blosc_VERSION} found ...")
  add_custom_target( Blosc COMMAND echo "Blosc build exists, skipping rebuild ...")

else()

  set(BLOSC_PREFIX blosc)

  if( LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(BLOSC_BUILD_STATIC ON)
    set(BLOSC_BUILD_SHARED OFF)
  else()
    set(BLOSC_BUILD_STATIC OFF)
    set(BLOSC_BUILD_SHARED ON)
  endif()

  ExternalProject_Add( Blosc
    PREFIX ${EXTERNALS_SOURCE_DIR}/${BLOSC_PREFIX}
    GIT_REPOSITORY https://github.com/Blosc/c-blosc.git
    GIT_TAG v1.21.0
    UPDATE_DISCONNECTED true
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_BUILD_DIR}
    CMAKE_ARGS -D BUILD_STATIC:BOOL=${BLOSC_BUILD_STATIC}
    CMAKE_ARGS -D BUILD_SHARED:BOOL=${BLOSC_BUILD_SHARED}
    CMAKE_ARGS -D BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D BUILD_FUZZERS:BOOL=OFF
    CMAKE_ARGS -D BUILD_BENCHMARKS:BOOL=OFF
    CMAKE_ARGS <SOURCE_DIR>
  )
endif()

#######################
# Pybind11
#######################

set(pybind11_ROOT ${EXTERNALS_BUILD_DIR})
find_package(pybind11 2.5.0 EXACT)

if( pybind11_FOUND )

  message("pybind11 version ${pybind11_VERSION} found ...")
  add_custom_target( PyBind COMMAND echo "PyBind11 build exists, skipping rebuild ...")

else()

  set(PYBIND_PREFIX pybind11)

  ExternalProject_Add ( PyBind
    PREFIX ${EXTERNALS_SOURCE_DIR}/${PYBIND_PREFIX}
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.5.0
    UPDATE_DISCONNECTED true
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_BUILD_DIR}
    CMAKE_ARGS -D PYBIND11_TEST:BOOL=OFF
    CMAKE_ARGS -D OIIO_BUILD_TOOLS:BOOL=OFF
    CMAKE_ARGS -D BUILD_DOCS:BOOL=OFF
    CMAKE_ARGS <SOURCE_DIR>
  )

endif()

#######################
# Boost
#######################

set(EXTERNAL_BOOST_VERSION 1.73.0)
set(BOOST_ROOT ${EXTERNALS_BUILD_DIR})
find_package(Boost ${EXTERNAL_BOOST_VERSION} EXACT COMPONENTS filesystem system date_time iostreams program_options)

if(Boost_FOUND)

  message("Boost version ${Boost_VERSION} found ...")
  add_custom_target( Boost COMMAND echo "Boost ${EXTERNAL_BOOST_VERSION} build exists, skipping rebuild ...")

else()
  
  message("Boost not found! Running external build ...")
  set(BOOST_PREFIX boost)

  if( LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(BOOST_LINK_STYLE static)
  else()
    set(BOOST_LINK_STYLE shared)
  endif()

  ExternalProject_Add ( Boost
    PREFIX ${EXTERNALS_SOURCE_DIR}/${BOOST_PREFIX}
    GIT_REPOSITORY https://github.com/boostorg/boost.git
    GIT_TAG boost-${EXTERNAL_BOOST_VERSION}
    UPDATE_DISCONNECTED true
    CONFIGURE_COMMAND ./bootstrap.sh
      --with-libraries=filesystem
      --with-libraries=system
      --with-libraries=date_time
      --with-libraries=iostreams
      --with-libraries=program_options
      --with-libraries=regex
      --with-libraries=log 
      --with-libraries=thread
      --prefix=<INSTALL_DIR>
    BUILD_COMMAND ./b2 install link=${BOOST_LINK_STYLE} variant=release threading=multi runtime-link=${BOOST_LINK_STYLE}
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
    INSTALL_DIR ${EXTERNALS_BUILD_DIR}
  )

endif()

#######################
# Imath
#######################

#set(Imath_ROOT ${EXTERNALS_BUILD_DIR})
#find_package(Imath)

#if( Imath_FOUND )

#  message("Imath version ${Imath_VERSION} found ...")
#  add_custom_target( Imath COMMAND echo "Imath build exists, skipping rebuild ...")

#else()

#  message("Imath not found! Running external build ...")
#  set(IMATH_PREFIX imath)

#  ExternalProject_Add( Imath
#    PREFIX ${EXTERNALS_SOURCE_DIR}/${IMATH_PREFIX}
#    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/Imath.git
#    GIT_TAG v3.1.3
#    UPDATE_DISCONNECTED true
#    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_BUILD_DIR}
#    CMAKE_ARGS <SOURCE_DIR>
#  )

#endif()

#######################
# OpenEXR
#######################

set(EXTERNAL_OPENEXR_VERSION 2.5.7)
set(OpenEXR_ROOT ${EXTERNALS_BUILD_DIR})
find_package(OpenEXR CONFIG)

if( OpenEXR_FOUND )

  message("OpenEXR version ${OpenEXR_VERSION} found ...")
  add_custom_target( OpenEXR COMMAND echo "OpenEXR build exists, skipping rebuild ...")

else()

  message("OpenEXR not found! Running external build ...")
  set(OPENEXR_PREFIX openexr)

  if( LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(OPENEXR_BUILD_SHARED OFF)
  else()
    set(OPENEXR_BUILD_SHARED ON)
  endif()

  ExternalProject_Add( OpenEXR
    PREFIX ${EXTERNALS_SOURCE_DIR}/${OPENEXR_PREFIX}
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openexr.git
    GIT_TAG v${EXTERNAL_OPENEXR_VERSION}
    UPDATE_DISCONNECTED true
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_BUILD_DIR}
    CMAKE_ARGS -D BUILD_TESTING:BOOL=OFF
    CMAKE_ARGS -D BUILD_SHARED_LIBS:BOOL=${OPENEXR_BUILD_SHARED}
    CMAKE_ARGS -D PYILMBASE_ENABLE:BOOL=OFF
    CMAKE_ARGS <SOURCE_DIR>
  )
endif()
#add_dependencies(OpenEXR Imath)

#######################
# Assimp
#######################

set(Assimp_ROOT ${EXTERNALS_BUILD_DIR})
find_package(Assimp)

if(Assimp_FOUND)

  message("Assimp version ${Assimp_VERSION} found ...")
  add_custom_target( Assimp COMMAND echo "Assimp build exists, skipping rebuild ...")

else()

  message("Assimp not found! Running external build ...")
  set(ASSIMP_PREFIX assimp)

  if( LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(ASSIMP_BUILD_SHARED OFF)
  else()
    set(ASSIMP_BUILD_SHARED ON)
  endif()

  ExternalProject_Add ( Assimp
    PREFIX ${EXTERNALS_SOURCE_DIR}/${ASSIMP_PREFIX}
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.0.0
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D BUILD_SHARED_LIBS:BOOL=${ASSIMP_BUILD_SHARED}
    CMAKE_ARGS -D ASSIMP_BUILD_ASSIMP_TOOLS:BOOL=OFF
    CMAKE_ARGS -D ASSIMP_BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D ASSIMP_INSTALL:BOOL=OFF
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_BUILD_DIR}
    CMAKE_ARGS <SOURCE_DIR>
  )

endif()

#######################
# OpenVDB
#######################

set(OpenVDB_ROOT ${EXTERNALS_BUILD_DIR})
find_package(OpenVDB)

if( OpenVDB_FOUND )

  message("OpenVDB version ${OpenVDB_VERSION} found ...")
  add_custom_target( OpenVDB COMMAND echo "OpenVDB build exists, skipping rebuild ...")

else()

  message("OpenVDB not found! Running external build ...")
  set(OPENVDB_PREFIX openvdb)

  if( LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(OPENVDB_USE_BOOST_STATIC_RUNTIME ON)
    set(OPENVDB_BUILD_CORE_SHARED OFF)
  else()
    set(OPENVDB_USE_BOOST_STATIC_RUNTIME OFF)
    set(OPENVDB_BUILD_CORE_SHARED ON)
  endif()

  ExternalProject_Add ( OpenVDB
    PREFIX ${EXTERNALS_SOURCE_DIR}/${OPENVDB_PREFIX}
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openvdb.git
    GIT_REMOTE_NAME origin/feature/nanovdb
    UPDATE_DISCONNECTED true
    CMAKE_ARGS -D OPENVDB_BUILD_NANOVDB:BOOL=ON
    CMAKE_ARGS -D DISABLE_CMAKE_SEARCH_PATHS:BOOL=ON
    CMAKE_ARGS -D Boost_USE_STATIC_RUNTIME:BOOL=${OPENVDB_USE_BOOST_STATIC_RUNTIME}
    CMAKE_ARGS -D OPENVDB_BUILD_BINARIES:BOOL=OFF
    CMAKE_ARGS -D OPENVDB_ENABLE_UNINSTALL:BOOL=OFF
    CMAKE_ARGS -D OPENVDB_CORE_SHARED:BOOL=${OPENVDB_BUILD_CORE_SHARED} 
    CMAKE_ARGS -D TBB_ROOT:PATH=${EXTERNALS_BUILD_DIR}
    CMAKE_ARGS -D BLOSC_ROOT:PATH=${EXTERNALS_BUILD_DIR}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_BUILD_DIR}
    CMAKE_ARGS <SOURCE_DIR>
  )

endif()
add_dependencies(OpenVDB Tbb Blosc Boost)

#######################
# OpenImageIO
#######################

set(OpenImageIO_ROOT ${EXTERNALS_BUILD_DIR})
find_package(OpenImageIO 2.2.18.0 EXACT)

if(OpenImageIO_FOUND)

  message("OpenImageIO version ${OPENIMAGEIO_VERSION} found ...")
  add_custom_target( OpenImageIO COMMAND echo "OpenImageIO build exists, skipping rebuild ...")

else()

  message("OpenImageIO not found! Running external build ...")
  set(OIIO_PREFIX openimageio)

  ExternalProject_Add ( OpenImageIO
    PREFIX ${EXTERNALS_SOURCE_DIR}/${OIIO_PREFIX}
    GIT_REPOSITORY https://github.com/OpenImageIO/oiio.git
    GIT_TAG v2.2.18.0
    UPDATE_DISCONNECTED true
    CMAKE_ARGS -D CMAKE_CXX_STANDARD=14
    CMAKE_ARGS -D OIIO_BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D OIIO_BUILD_TOOLS:BOOL=OFF
    CMAKE_ARGS -D BUILD_DOCS:BOOL=OFF
    CMAKE_ARGS -D Boost_ROOT=${EXTERNALS_BUILD_DIR}
    CMAKE_ARGS -D TBB_ROOT=${EXTERNALS_BUILD_DIR}
    CMAKE_ARGS -D OpenEXR_ROOT=${EXTERNALS_BUILD_DIR}
    CMAKE_ARGS -D pybind11_ROOT=${EXTERNALS_BUILD_DIR}
    CMAKE_ARGS -D USE_PYTHON:BOOL=OFF
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_BUILD_DIR}
    CMAKE_ARGS <SOURCE_DIR>
  )

endif()
add_dependencies(OpenImageIO PyBind OpenEXR OpenVDB)

########################
# Slang section
########################

set(Slang_ROOT ${EXTERNALS_BUILD_DIR})
find_package(Slang)

if( Slang_FOUND )
  message("Slang version ${Slang_VERSION} found ...")
  add_custom_target( Slang COMMAND echo "Slang build exists, skipping rebuild ...")
else()

set(EXTERNAL_SLANG_VERSION 0.19.14) # last working version 0.18.6
set(SLANG_PREFIX slang)
set(SLANG_PLATFORM linux-x64)
set(SLANG_BUILD_LIBS "slang slang-glslang")

set(SLANG_RELEASE_BIN_DIR "bin/${SLANG_PLATFORM}/release")
set(SLANG_DEBUG_BIN_DIR "bin/${SLANG_PLATFORM}/debug")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(SLANG_BUILD_CONFIG release_x64)
  set(SLANG_BIN_DIR ${SLANG_RELEASE_BIN_DIR})
else()
  set(SLANG_BUILD_CONFIG debug_x64)
  set(SLANG_BIN_DIR ${SLANG_DEBUG_BIN_DIR})
endif()

set(SLANG_RELEASE_BIN_DIR "bin/${SLANG_PLATFORM}/release")
set(SLANG_DEBUG_BIN_DIR "bin/${SLANG_PLATFORM}/debug")

string(REPLACE " " ";" SLANG_BUILD_LIBS_LIST ${SLANG_BUILD_LIBS})

ExternalProject_Add( Slang
  PREFIX ${EXTERNALS_SOURCE_DIR}/${SLANG_PREFIX}
  GIT_REPOSITORY https://github.com/shader-slang/slang.git
  GIT_REMOTE_NAME releases/tags/v${EXTERNAL_SLANG_VERSION}
  UPDATE_DISCONNECTED ON
  BUILD_IN_SOURCE   TRUE
  LOG_BUILD ON
  USES_TERMINAL ON
  USES_TERMINAL_CONFIGURE ON

  CONFIGURE_COMMAND 
    COMMAND ${CMAKE_COMMAND}
      -D GIT_EXECUTABLE=${GIT_EXECUTABLE}
      -D INPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/slang-tag-version.h.in
      -D OUTPUT_FILE=<SOURCE_DIR>/slang-tag-version.h
      -P ${CMAKE_CURRENT_SOURCE_DIR}/slang_generate_version.cmake
    COMMAND ./premake5 gmake --deps=true --build-glslang=true --enable-embed-stdlib=true --arch=x64
  
  BUILD_COMMAND ${MAKE_EXECUTABLE} ${SLANG_BUILD_LIBS_LIST} config=${SLANG_BUILD_CONFIG}
  INSTALL_COMMAND ${CMAKE_COMMAND}
    -D SLANG_LIBS_LIST=${SLANG_BUILD_LIBS}
    -D SOURCE_DIR=<SOURCE_DIR>
    -D BIN_DIR=<SOURCE_DIR>/${SLANG_BIN_DIR}/
    -D DEST_DIR=${EXTERNALS_BUILD_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/slang_install_files.cmake
)

ExternalProject_Add_Step(
  Slang download_premake5
  COMMENT "Downloading premake5 ..."
  ALWAYS ON
  COMMAND ${CMAKE_COMMAND}
    -D PREMAKE_URL=https://github.com/shader-slang/slang-binaries/blob/master/premake/premake-5.0.0-alpha16/bin/linux-64/premake5?raw=true
    -D OUTPUT_FILE=<SOURCE_DIR>/premake5
    -P ${CMAKE_CURRENT_SOURCE_DIR}/slang_download_premake.cmake
  DEPENDERS configure build install
)

endif()

add_custom_target( dummy_target COMMAND echo "Third party project dummy target")
add_dependencies( dummy_target 
  Tbb 
  Blosc
  PyBind
  Assimp
  OpenEXR
  OpenVDB
  OpenImageIO
  Slang
)