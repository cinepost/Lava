cmake_minimum_required(VERSION 3.20)

project("ThirdParty_External")

include(ExternalProject)
include(ProcessorCount)

message("Running third party dependencies build...")

#we need git executable
find_package(Git REQUIRED)

set( CMAKE_MODULE_PATH 
  ${PROJECT_SOURCE_DIR}/../cmake/modules
)

include(${PROJECT_SOURCE_DIR}/external_versions.cmake)

set(EXTRA_CMAKE_ARGS)

set(EXTERNALS_BINARY_DIR ${PROJECT_SOURCE_DIR}/../build/${DEPS_BUILD_TYPE}/external)
set(EXTERNALS_STAMP_DIR ${PROJECT_SOURCE_DIR}/../build/${DEPS_BUILD_TYPE}/external/stamp)
set(EXTERNALS_TMP_DIR ${PROJECT_SOURCE_DIR}/../build/${DEPS_BUILD_TYPE}/external/tmp)

set(EXTERNALS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)

find_program(MAKE_EXECUTABLE
  NAMES gmake mingw32-make make
  NAMES_PER_DIR
  DOC "GNU Make"
)

if(WIN32)
  set(MAKE_EXECUTABLE cmake --build . -j 8 --config ${DEPS_BUILD_TYPE})
endif()

find_program(CHMOD_EXECUTABLE
  NAMES chmod
  NAMES_PER_DIR
)

find_package(Git)
if(Git_FOUND)
  message("Git found: ${GIT_EXECUTABLE}")
endif()

#######################
# ZLIB
#######################

if( NOT EXTERNAL_ZLIB_VERSION )
  set(EXTERNAL_ZLIB_VERSION 1.2.11)
endif()

unset(ZLIB_FOUND)
unset(ZLIB_LIBRARY)
unset(ZLIB_INCLUDE_DIR)

if(NOT LAVA_3RD_LIB_LINK_WITH_STATIC)
  set(ZLIB_USE_STATIC_LIBS OFF)
endif()

set(ZLIB_ROOT ${EXTERNALS_INSTALL_DIR})
find_package( ZLIB ${EXTERNAL_ZLIB_VERSION} EXACT)

if( ZLIB_FOUND )

  message("ZLIB version ${ZLIB_VERSION_STRING} found ...")
  add_custom_target( zlib COMMAND echo "ZLIB build exists, skipping rebuild ...")

else()

  message("ZLIB version ${ZLIB_VERSION_STRING} not found! Running external build ...")
  set(ZLIB_PREFIX zlib)

  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(ZLIB_USE_STATIC_LIBS ON)
    set(ZLIB_BUILD_SHARED OFF)
  else()
    set(ZLIB_USE_STATIC_LIBS OFF)
    set(ZLIB_BUILD_SHARED ON)
  endif()

  ExternalProject_Add( zlib
    PREFIX ${EXTERNALS_SOURCE_DIR}/${ZLIB_PREFIX}
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v${EXTERNAL_ZLIB_VERSION}
    UPDATE_DISCONNECTED true
    
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D SKIP_INSTALL_LIBRARIES:BOOL=OFF
    CMAKE_ARGS -D CMAKE_CXX_FLAGS=-D_GLIBCXX_USE_CXX11_ABI=0
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/${ZLIB_PREFIX}
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/${ZLIB_PREFIX}
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/${ZLIB_PREFIX}
  )
endif()

#######################
# TBB
#######################

if( NOT EXTERNAL_TBB_VERSION )
  if(WIN32)
  set(EXTERNAL_TBB_VERSION 2021.8.0)  
  else()
  set(EXTERNAL_TBB_VERSION 2020.2)
  endif()
endif()

set(TBB_ROOT ${EXTERNALS_INSTALL_DIR})
find_package( TBB ${EXTERNAL_TBB_VERSION} EXACT)

if( TBB_FOUND )

  message("TBB version ${Tbb_VERSION} found ...")
  add_custom_target( Tbb COMMAND echo "TBB build exists, skipping rebuild ...")
else()

  message("TBB not found! Running external build ...")
  set(TBB_PREFIX tbb)

  if(WIN32)
  ExternalProject_Add( Tbb
    PREFIX ${EXTERNALS_SOURCE_DIR}/${TBB_PREFIX}
    GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
    GIT_TAG v${EXTERNAL_TBB_VERSION}
    UPDATE_DISCONNECTED true
    BUILD_IN_SOURCE 1
    
    # CONFIGURE_COMMAND ""
    # CONFIGURE_COMMAND cmake -DTBB_BUILD_DIR=${EXTERNALS_BINARY_DIR}/tbb/build -DTBB_BUILD_PREFIX=${TBB_PREFIX}
    CMAKE_ARGS -D TBB_BUILD_DIR=${EXTERNALS_BINARY_DIR}/tbb/build
    CMAKE_ARGS -D TBB_BUILD_PREFIX=${TBB_PREFIX}
    # CMAKE_ARGS -D CMAKE_INSTALL_PREFIX=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>
    
    # BUILD_COMMAND ${MAKE_EXECUTABLE} tbb_build_dir=${EXTERNALS_BINARY_DIR}/tbb/build tbb_build_prefix=${TBB_PREFIX}
    # BUILD_COMMAND cmake --build . -j 8 --config Release
    BUILD_COMMAND ${MAKE_EXECUTABLE}

    # INSTALL_DIR ${EXTERNALS_INSTALL_DIR}

    INSTALL_COMMAND cmake --install . --config ${DEPS_BUILD_TYPE}

    # INSTALL_COMMAND ${CMAKE_COMMAND}
    #   -D TBB_LIBS_DIR=${EXTERNALS_BINARY_DIR}/tbb/build/${TBB_PREFIX}_release
    #   -D TBB_INCL_DIR=<SOURCE_DIR>/include/tbb
    #   -D DEST_DIR=${EXTERNALS_INSTALL_DIR}
    #   -P ${CMAKE_CURRENT_SOURCE_DIR}/tbb_install_files.cmake

    TMP_DIR ${EXTERNALS_TMP_DIR}/tbb
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/tbb
  )
  else()
  ExternalProject_Add( Tbb
    PREFIX ${EXTERNALS_SOURCE_DIR}/${TBB_PREFIX}
    GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
    GIT_TAG v${EXTERNAL_TBB_VERSION}
    UPDATE_DISCONNECTED true
    BUILD_IN_SOURCE 1
    
    CONFIGURE_COMMAND ""
    
    BUILD_COMMAND ${MAKE_EXECUTABLE} tbb_build_dir=${EXTERNALS_BINARY_DIR}/tbb/build tbb_build_prefix=${TBB_PREFIX}

    INSTALL_DIR ${EXTERNALS_INSTALL_DIR}
    
    INSTALL_COMMAND ${CMAKE_COMMAND}
      -D TBB_LIBS_DIR=${EXTERNALS_BINARY_DIR}/tbb/build/${TBB_PREFIX}_release
      -D TBB_INCL_DIR=<SOURCE_DIR>/include/tbb
      -D DEST_DIR=${EXTERNALS_INSTALL_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tbb_install_files.cmake

    TMP_DIR ${EXTERNALS_TMP_DIR}/tbb
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/tbb
  )
  endif()

endif()



#######################
# Blosc2
#######################

if( NOT EXTERNAL_BLOSC_VERSION)
  set(EXTERNAL_BLOSC_VERSION 1.21.0)
endif()

set(Blosc_ROOT ${EXTERNALS_INSTALL_DIR})
set(BLOSC_USE_STATIC_LIBS ON)
find_package(Blosc ${EXTERNAL_BLOSC_VERSION} EXACT)

if( Blosc_FOUND )

  message("Blosc version ${EXTERNAL_BLOSC_VERSION} found ...")
  add_custom_target( Blosc COMMAND echo "Blosc build exists, skipping rebuild ...")

else()

  message("Blosc version ${EXTERNAL_BLOSC_VERSION} not found! Running external build ...")
  set(BLOSC_PREFIX blosc)

  # Always build Blosc as a static lib
  set(BLOSC_BUILD_STATIC ON)
  set(BLOSC_BUILD_SHARED OFF)

#  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
#    set(BLOSC_BUILD_STATIC ON)
#    set(BLOSC_BUILD_SHARED OFF)
#  else()
#    set(BLOSC_BUILD_STATIC OFF)
#    set(BLOSC_BUILD_SHARED ON)
#  endif()

  ExternalProject_Add( Blosc
    PREFIX ${EXTERNALS_SOURCE_DIR}/${BLOSC_PREFIX}
    GIT_REPOSITORY https://github.com/Blosc/c-blosc.git
    GIT_TAG v${EXTERNAL_BLOSC_VERSION}
    UPDATE_DISCONNECTED true
    
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D BUILD_STATIC:BOOL=${BLOSC_BUILD_STATIC}
    CMAKE_ARGS -D BUILD_SHARED:BOOL=${BLOSC_BUILD_SHARED}
    CMAKE_ARGS -D BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D BUILD_FUZZERS:BOOL=OFF
    CMAKE_ARGS -D BUILD_BENCHMARKS:BOOL=OFF
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_CXX_FLAGS:STRING="-fPIC"
    CMAKE_ARGS -D CMAKE_C_FLAGS:STRING="-fPIC"
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/blosc
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/blosc
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/blosc
  )

endif()


#######################
# Boost
#######################

set(Boost_USE_MULTITHREADED ON)

if(LAVA_3RD_LIB_LINK_WITH_STATIC)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
endif()

if( NOT EXTERNAL_BOOST_VERSION )
  set(EXTERNAL_BOOST_VERSION 1.75.0)
endif()

set(BOOST_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(Boost ${EXTERNAL_BOOST_VERSION} EXACT COMPONENTS json filesystem system date_time iostreams program_options)

if(Boost_FOUND)

  message("Boost version ${Boost_VERSION} found ...")
  message(Boost_INCLUDE_DIR "${Boost_INCLUDE_DIR}")
  add_custom_target( Boost COMMAND echo "Boost ${EXTERNAL_BOOST_VERSION} build exists, skipping rebuild ...")

else()
  
  message("Boost version ${EXTERNAL_BOOST_VERSION} not found! Running external build ...")
  set(BOOST_PREFIX boost)

  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(BOOST_LINK_STYLE static)
    set(BOOST_CXX_FLAGS "cxxflags=-fPIC")
    set(BOOST_C_FLAGS "cflags=-fPIC")
  else()
    set(BOOST_LINK_STYLE shared)
    set(BOOST_CXX_FLAGS "")
    set(BOOST_C_FLAGS "")
  endif()

  if(DEPS_BUILD_TYPE STREQUAL "Debug")
    set(BOOST_BUILD_TYPE "debug")
  else()
    set(BOOST_BUILD_TYPE "release")
  endif()

  if(WIN32)
  ExternalProject_Add ( Boost
    PREFIX ${EXTERNALS_SOURCE_DIR}/${BOOST_PREFIX}
    GIT_REPOSITORY https://github.com/boostorg/boost.git
    GIT_TAG boost-${EXTERNAL_BOOST_VERSION}
    UPDATE_DISCONNECTED true
    # CONFIGURE_COMMAND ./bootstrap.sh
    #   --with-libraries=filesystem
    #   --with-libraries=system
    #   --with-libraries=date_time
    #   --with-libraries=iostreams
    #   --with-libraries=program_options
    #   --with-libraries=regex
    #   --with-libraries=log 
    #   --with-libraries=thread
    #   --with-libraries=json
    #   --prefix=<INSTALL_DIR>
    CONFIGURE_COMMAND ./bootstrap.bat
    BUILD_COMMAND ./b2 define=_GLIBCXX_USE_CXX11_ABI=0 --disable-icu --build-dir=${EXTERNALS_BINARY_DIR}/boost install -j8 toolset=msvc-14.2 address-model=64 architecture=x86 link=${BOOST_LINK_STYLE} variant=${BOOST_BUILD_TYPE} threading=multi runtime-link=${BOOST_LINK_STYLE} ${BOOST_CXX_FLAGS} ${BOOST_C_FLAGS}
      --with-filesystem
      --with-system
      --with-date_time
      --with-iostreams
      --with-program_options
      --with-regex
      --with-log 
      --with-thread
      --with-json
      --prefix=${EXTERNALS_INSTALL_DIR}
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
    INSTALL_DIR ${EXTERNALS_INSTALL_DIR}
    TMP_DIR ${EXTERNALS_TMP_DIR}/boost
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/boost
  )
  else()
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
      --with-libraries=json
      --prefix=<INSTALL_DIR>
    BUILD_COMMAND ./b2 define=_GLIBCXX_USE_CXX11_ABI=0 --disable-icu --build-dir=${EXTERNALS_BINARY_DIR}/boost install link=${BOOST_LINK_STYLE} variant=${BOOST_BUILD_TYPE} threading=multi runtime-link=${BOOST_LINK_STYLE} ${BOOST_CXX_FLAGS} ${BOOST_C_FLAGS}
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
    INSTALL_DIR ${EXTERNALS_INSTALL_DIR}
    TMP_DIR ${EXTERNALS_TMP_DIR}/boost
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/boost
  )
  endif()
endif()

#######################
# Pybind11
#######################

if( NOT EXTERNAL_PYBIND_VERSION )
  set(EXTERNAL_PYBIND_VERSION 2.5.0)
endif()

set(pybind11_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(pybind11 ${EXTERNAL_PYBIND_VERSION} EXACT)

if( pybind11_FOUND )

  message("pybind11 version ${pybind11_VERSION} found ...")
  add_custom_target( PyBind COMMAND echo "PyBind11 build exists, skipping rebuild ...")

else()

  message("pybind11 version ${EXTERNAL_PYBIND_VERSION} not found! Running external build ...")
  set(PYBIND_PREFIX pybind11)

  ExternalProject_Add ( PyBind
    PREFIX ${EXTERNALS_SOURCE_DIR}/${PYBIND_PREFIX}
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v${EXTERNAL_PYBIND_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D PYBIND11_TEST:BOOL=OFF
    CMAKE_ARGS -D OIIO_BUILD_TOOLS:BOOL=OFF
    CMAKE_ARGS -D BUILD_DOCS:BOOL=OFF
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/pybind
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/pybind
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/pybind
  )

endif()


#######################
# OpenEXR
#######################

if( NOT EXTERNAL_OPENEXR_VERSION )
  set(EXTERNAL_OPENEXR_VERSION 2.5.8)
endif()

if(LAVA_3RD_LIB_LINK_WITH_STATIC)
set(OPENEXR_USE_STATIC_LIBS ON)
endif()

set(OpenEXR_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(OpenEXR ${EXTERNAL_OPENEXR_VERSION} EXACT)

if( OpenEXR_FOUND )

  message("OpenEXR version ${OpenEXR_VERSION} found ...")
  add_custom_target( OpenEXR COMMAND echo "OpenEXR build exists, skipping rebuild ...")

else()

  message("OpenEXR not found! Running external build ...")
  set(OPENEXR_PREFIX openexr)

  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(OPENEXR_BUILD_SHARED OFF)
  else()
    set(OPENEXR_BUILD_SHARED ON)
  endif()

  ExternalProject_Add( OpenEXR
    PREFIX ${EXTERNALS_SOURCE_DIR}/${OPENEXR_PREFIX}
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openexr.git
    GIT_TAG v${EXTERNAL_OPENEXR_VERSION}
    UPDATE_DISCONNECTED true
    
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D BUILD_TESTING:BOOL=OFF
    CMAKE_ARGS -D OPENEXR_BUILD_UTILS:BOOL=OFF
    CMAKE_ARGS -D BUILD_SHARED_LIBS:BOOL=${OPENEXR_BUILD_SHARED}
    CMAKE_ARGS -D PYILMBASE_ENABLE:BOOL=OFF
    CMAKE_ARGS -D CMAKE_CXX_FLAGS=-D_GLIBCXX_USE_CXX11_ABI=0
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/openexr
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/openexr
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/openexr
  )
  ExternalProject_Add_StepDependencies(OpenEXR configure zlib Tbb OpenVDB Blosc)

endif()


#######################
# Assimp
#######################

if( NOT EXTERNAL_ASSIMP_VERSION )
  set(EXTERNAL_ASSIMP_VERSION 5.0.0)
endif()

set(assimp_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(assimp ${EXTERNAL_ASSIMP_VERSION} EXACT)

if(assimp_FOUND)
  message("Assimp version ${assimp_VERSION} found ...")
  add_custom_target( Assimp COMMAND echo "Assimp build exists, skipping rebuild ...")
else()

  message("Assimp version ${EXTERNAL_ASSIMP_VERSION} not found! Running external build ...")
  set(ASSIMP_PREFIX assimp)

  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(ASSIMP_BUILD_SHARED OFF)
  else()
    set(ASSIMP_BUILD_SHARED ON)
  endif()

  ExternalProject_Add ( Assimp
    PREFIX ${EXTERNALS_SOURCE_DIR}/${ASSIMP_PREFIX}
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v${EXTERNAL_ASSIMP_VERSION}
    UPDATE_DISCONNECTED true
    
    CMAKE_ARGS -D BUILD_SHARED_LIBS:BOOL=${ASSIMP_BUILD_SHARED}
    CMAKE_ARGS -D ASSIMP_BUILD_ASSIMP_TOOLS:BOOL=OFF
    CMAKE_ARGS -D ASSIMP_BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D ASSIMP_INSTALL:BOOL=ON
    CMAKE_ARGS -D ASSIMP_BUILD_OBJ_IMPORTER:BOOL=TRUE
    CMAKE_ARGS -D ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT:BOOL=OFF
    CMAKE_ARGS -D ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT:BOOL=OFF
    CMAKE_ARGS -D ASSIMP_INSTALL_PDB:BOOL=OFF
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/assimp
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/assimp
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/assimp
  )
  ExternalProject_Add_StepDependencies(Assimp configure zlib Tbb OpenVDB Blosc)

endif()


#######################
# OpenVDB
#######################

if( NOT EXTERNAL_OPENVDB_VERSION)
  set(EXTERNAL_OPENVDB_VERSION 9.0.0)
endif()

if(LAVA_3RD_LIB_LINK_WITH_STATIC)
set(OPENVDB_USE_STATIC_LIBS ON)
endif()

set(OpenVDB_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(OpenVDB ${EXTERNAL_OPENVDB_VERSION} EXACT)

if( OpenVDB_FOUND )

  message("OpenVDB version ${OpenVDB_VERSION} found ...")
  add_custom_target( OpenVDB COMMAND echo "OpenVDB build exists, skipping rebuild ...")

else()

  message("OpenVDB version ${EXTERNAL_OPENVDB_VERSION} not found! Running external build ...")
  set(OPENVDB_PREFIX openvdb)

  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(OPENVDB_USE_BOOST_STATIC_RUNTIME ON)
    set(OPENVDB_BUILD_CORE_SHARED OFF)
    set(OPENVDB_CXX_FLAGS "-fPIC")
    set(OPENVDB_C_FLAGS "-fPIC")
  else()
    set(OPENVDB_USE_BOOST_STATIC_RUNTIME OFF)
    set(OPENVDB_BUILD_CORE_SHARED ON)
    set(OPENVDB_CXX_FLAGS "-D_GLIBCXX_USE_CXX11_ABI=0")
  endif()

  if(WIN32)
  set(OPENVDB_CXX_FLAGS "/EHsc")
  if(DEPS_BUILD_TYPE STREQUAL "Release")
    set(OPENVDB_CXX_STANDARD_LIBRARIES "D:/Lava/deps/Release/lib/tbb12.lib")
  else()
    set(OPENVDB_CXX_STANDARD_LIBRARIES "D:/Lava/deps/Debug/lib/tbb12_debug.lib")
  endif()
  elseif(UNIX)
    # set(OPENVDB_CXX_STANDARD_LIBRARIES "")
  endif()

  ExternalProject_Add ( OpenVDB
    PREFIX ${EXTERNALS_SOURCE_DIR}/${OPENVDB_PREFIX}
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openvdb.git
    GIT_TAG v${EXTERNAL_OPENVDB_VERSION}
    
    CMAKE_ARGS -D USE_NANOVDB:BOOL=ON
    CMAKE_ARGS -D USE_BLOSC:BOOL=OFF
    CMAKE_ARGS -D USE_TBB:BOOL=OFF
    CMAKE_ARGS -D USE_EXPLICIT_INSTANTIATION:BOOL=OFF

    CMAKE_ARGS -D OPENVDB_BUILD_AX:BOOL=OFF
    CMAKE_ARGS -D OPENVDB_BUILD_CORE:BOOL=ON
    CMAKE_ARGS -D OPENVDB_BUILD_NANOVDB:BOOL=ON
    
    CMAKE_ARGS -D DISABLE_CMAKE_SEARCH_PATHS:BOOL=ON
    CMAKE_ARGS -D Boost_USE_STATIC_RUNTIME:BOOL=${OPENVDB_USE_BOOST_STATIC_RUNTIME}
    CMAKE_ARGS -D OPENVDB_BUILD_BINARIES:BOOL=OFF
    CMAKE_ARGS -D OPENVDB_ENABLE_UNINSTALL:BOOL=OFF
    CMAKE_ARGS -D OPENVDB_CORE_SHARED:BOOL=${OPENVDB_BUILD_CORE_SHARED} 
    
    CMAKE_ARGS -D Blosc_ROOT:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D Blosc_LIBRARY:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D Blosc_INCLUDE_DIR:PATH=${EXTERNALS_INSTALL_DIR}/include/ 

    CMAKE_ARGS -D Tbb_ROOT:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D TBB_ROOT:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D TBB_INCLUDEDIR:PATH=${EXTERNALS_INSTALL_DIR}/include/
    CMAKE_ARGS -D TBB_LIBRARYDIR:PATH=${EXTERNALS_INSTALL_DIR}/lib/
    
    CMAKE_ARGS -D CMAKE_CXX_FLAGS=${OPENVDB_CXX_FLAGS}
    CMAKE_ARGS $<$<PLATFORM_ID:Windows>:-DCMAKE_CXX_STANDARD_LIBRARIES=${OPENVDB_CXX_STANDARD_LIBRARIES}>
    CMAKE_ARGS -D CMAKE_C_FLAGS=${OPENVDB_C_FLAGS}
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/openvdb
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/openvdb
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/openvdb
  )
  ExternalProject_Get_property(OpenVDB SOURCE_DIR)
  ExternalProject_Add_StepDependencies(OpenVDB configure zlib Tbb Boost Blosc)

endif()


#######################
# PNG
#######################

if( NOT EXTERNAL_PNG_VERSION )
  set(EXTERNAL_PNG_VERSION 1.6.37)
endif()

set(PNG_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(PNG ${EXTERNAL_PNG_VERSION} EXACT)

if(PNG_FOUND)

  message("PNG found ...")
  add_custom_target( png COMMAND echo "PNG build exists, skipping rebuild ...")

else()

  message("PNG not found! Running external build ...")
  set(PNG_PREFIX png)

  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(PNG_BUILD_STATIC_LIB ON)
    set(PNG_BUILD_SHARED_LIB OFF)
  else()
    set(PNG_BUILD_STATIC_LIB OFF)
    set(PNG_BUILD_SHARED_LIB ON)
  endif()

  set(ZLIB_ROOT ${EXTERNALS_INSTALL_DIR})
  if(WIN32)
  set(ZLIB_LIBRARY ${EXTERNALS_INSTALL_DIR}/lib/zlib.lib)
  else()
  find_package(ZLIB ${EXTERNAL_ZLIB_VERSION} EXACT REQUIRED)
  message("ZLIB_LIBRARY " ${ZLIB_LIBRARY})
  endif()
  
  ExternalProject_Add ( png
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libpng-${EXTERNAL_PNG_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D PNG_BUILD_ZLIB:BOOL=OFF
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D PNG_TESTS:BOOL=OFF
    CMAKE_ARGS -D PNG_STATIC:BOOL=${PNG_BUILD_STATIC_LIB}
    CMAKE_ARGS -D PNG_SHARED:BOOL=${PNG_BUILD_SHARED_LIB}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/${PNG_PREFIX}
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/${PNG_PREFIX}
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/${PNG_PREFIX}
  )
  ExternalProject_Add_StepDependencies(png configure zlib)
endif()

if(WIN32)
#######################
# JPEG
#######################

if( NOT EXTERNAL_JPEG_VERSION )
  set(EXTERNAL_JPEG_VERSION 2.1.0)
endif()

set(JPEG_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(JPEG ${EXTERNAL_JPEG_VERSION} EXACT)

if(JPEG_FOUND)

  message("JPEG found ...")
  add_custom_target( jpeg COMMAND echo "JPEG build exists, skipping rebuild ...")

else()

  message("JPEG not found! Running external build ...")
  set(JPEG_PREFIX jpeg)

  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(JPEG_BUILD_STATIC_LIB ON)
    set(JPEG_BUILD_SHARED_LIB OFF)
  else()
    set(JPEG_BUILD_STATIC_LIB OFF)
    set(JPEG_BUILD_SHARED_LIB ON)
  endif()

  ExternalProject_Add ( jpeg
    PREFIX ${EXTERNALS_SOURCE_DIR}/${JPEG_PREFIX}
    GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo
    GIT_TAG ${EXTERNAL_JPEG_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    # CMAKE_ARGS -D JPEG_TESTS:BOOL=OFF
    # CMAKE_ARGS -D JPEG_STATIC:BOOL=${JPEG_BUILD_STATIC_LIB}
    CMAKE_ARGS -D ENABLE_SHARED:BOOL=${JPEG_BUILD_SHARED_LIB}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    # BUILD_COMMAND ${MAKE_EXECUTABLE}
    # BUILD_COMMAND cmake --build . -j 8 --config Release
    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/${JPEG_PREFIX}
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/${JPEG_PREFIX}
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/${JPEG_PREFIX}
  )
  # ExternalProject_Add_StepDependencies(jpeg configure)
endif()
endif()

#######################
# OpenColorIO
#######################

if( NOT EXTERNAL_OCIO_VERSION )
  set(EXTERNAL_OCIO_VERSION 2.0.4)
endif()

set(OpenColorIO_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(OpenColorIO ${EXTERNAL_OCIO_VERSION} EXACT)

unset(OpenColorIO_FOUND)

if(OpenColorIO_FOUND)

  message("OpenColorIO version ${OpenColorIO_VERSION} found ...")
  add_custom_target( OpenColorIO COMMAND echo "OpenColorIO build exists, skipping rebuild ...")

elseif(NOT USE_OpenColorIO)
  add_custom_target( OpenColorIO COMMAND echo "OpenColorIO disabled, skipping build ...")
else()

  message("OpenColorIO not found! Running external build ...")
  set(OCIO_PREFIX opencolorio)

  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(OPENCOLORIO_BUILD_SHARED_LIBS OFF)
  else()
    set(OPENCOLORIO_BUILD_SHARED_LIBS ON)
  endif()

  if(DEPS_BUILD_TYPE STREQUAL "Release")
    set(OPENCOLORIO_BUILD_TYPE "Release")
  else()
    set(OPENCOLORIO_BUILD_TYPE "Debug")
  endif()

  ExternalProject_Add ( OpenColorIO
    PREFIX ${EXTERNALS_SOURCE_DIR}/${OCIO_PREFIX}
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/OpenColorIO.git
    GIT_TAG v${EXTERNAL_OCIO_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D BUILD_SHARED_LIBS:BOOL=${OPENCOLORIO_BUILD_SHARED_LIBS}
    CMAKE_ARGS -D OCIO_BUILD_APPS:BOOL=OFF
    CMAKE_ARGS -D OCIO_BUILD_OPENFX:BOOL=OFF
    CMAKE_ARGS -D OCIO_BUILD_GPU_TESTS:BOOL=OFF
    CMAKE_ARGS -D OCIO_USE_HEADLESS:BOOL=ON
    CMAKE_ARGS -D OCIO_BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D OCIO_BUILD_DOCS:BOOL=OFF
    CMAKE_ARGS -D OCIO_BUILD_PYTHON:BOOL=OFF

    CMAKE_ARGS -D CMAKE_CXX_FLAGS=-D_GLIBCXX_USE_CXX11_ABI=0
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${OPENCOLORIO_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/ocio
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/ocio
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/ocio
  )
endif()

#######################
# OpenImageIO
#######################

if( NOT EXTERNAL_OIIO_VERSION )
  set(EXTERNAL_OIIO_VERSION 2.2.18.0)
endif()

set(OPENIMAGEIO_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(OpenImageIO ${EXTERNAL_OIIO_VERSION} EXACT)

if(OpenImageIO_FOUND)

  message("OpenImageIO version ${OpenImageIO_VERSION} found ...")
  add_custom_target( OpenImageIO COMMAND echo "OpenImageIO build exists, skipping rebuild ...")

else()

  message("OpenImageIO not found! Running external build ...")
  set(OIIO_PREFIX openimageio)

  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(OPENIMAGEIO_LINK_STATIC ON)
    set(OPENIMAGEIO_BUILD_SHARED_LIBS OFF)
  else()
    set(OPENIMAGEIO_LINK_STATIC OFF)
    set(OPENIMAGEIO_BUILD_SHARED_LIBS ON)
  endif()

  set(BOOST_ROOT ${EXTERNALS_INSTALL_DIR})
  find_package(Boost ${EXTERNAL_BOOST_VERSION} EXACT)

  if(NOT LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(ZLIB_USE_STATIC_LIBS OFF)
  endif()
  set(ZLIB_ROOT ${EXTERNALS_INSTALL_DIR})
  find_package(ZLIB ${EXTERNAL_ZLIB_VERSION} EXACT)

  if(NOT TARGET ZLIB::ZLIB)
    message(WARNING "ZLIB library missing !!! Required by OpenimageIO")
  endif()

  set(PNG_ROOT ${EXTERNALS_INSTALL_DIR})
  find_package(PNG ${EXTERNAL_PNG_VERSION} EXACT)

  if(NOT TARGET PNG::PNG)
    message(WARNING "PNG library missing !!! Required by OpenimageIO")
  endif()

  set(OpenColorIO_ROOT ${EXTERNALS_INSTALL_DIR})
  find_package(OpenColorIO ${EXTERNAL_OCIO_VERSION} EXACT)

  ExternalProject_Add ( OpenImageIO
    PREFIX ${EXTERNALS_SOURCE_DIR}/${OIIO_PREFIX}
    GIT_REPOSITORY https://github.com/OpenImageIO/oiio.git
    GIT_TAG v${EXTERNAL_OIIO_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D CMAKE_CXX_STANDARD=14
    CMAKE_ARGS -D BUILD_SHARED_LIBS:BOOL=${OPENIMAGEIO_BUILD_SHARED_LIBS}
    CMAKE_ARGS -D LINKSTATIC:BOOL=${OPENIMAGEIO_LINK_STATIC}
    
    CMAKE_ARGS -D EMBEDPLUGINS:BOOL=ON
    CMAKE_ARGS -D OIIO_BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D OIIO_BUILD_TOOLS:BOOL=OFF
    CMAKE_ARGS -D BUILD_OIIOUTIL_ONLY:BOOL=OFF
    CMAKE_ARGS -D BUILD_DOCS:BOOL=OFF

    CMAKE_ARGS -D ZLIB_ROOT:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D Boost_ROOT:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D TBB_ROOT:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D OpenEXR_ROOT:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D OpenColorIO_ROOT:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D PNG_ROOT:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D pybind11_ROOT:PATH=${EXTERNALS_INSTALL_DIR}
    
    CMAKE_ARGS -D USE_FFMPEG:BOOL=OFF
    CMAKE_ARGS -D USE_RAW:BOOL=OFF
    CMAKE_ARGS -D USE_PYTHON:BOOL=OFF
    CMAKE_ARGS -D USE_FIELD3D:BOOL=OFF
    CMAKE_ARGS -D USE_OCIO:BOOL=OFF
    CMAKE_ARGS -D USE_OPENVDB:BOOL=OFF
    CMAKE_ARGS -D USE_OPENCOLORIO:BOOL=OFF
    CMAKE_ARGS -D USE_OPENCV:BOOL=OFF

    CMAKE_ARGS -D CMAKE_CXX_FLAGS=-D_GLIBCXX_USE_CXX11_ABI=0
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/oiio
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/oiio
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/oiio
  )
  if(WIN32)
  ExternalProject_Add_StepDependencies(OpenImageIO configure Tbb Blosc PyBind OpenEXR OpenVDB zlib png tiff jpeg)
  else()
  ExternalProject_Add_StepDependencies(OpenImageIO configure Tbb Blosc PyBind OpenEXR OpenVDB zlib png tiff)
  endif()

endif()


#######################
# GLFW3
#######################

if( NOT EXTERNAL_GLFW_VERSION )
  set(EXTERNAL_GLFW_VERSION 3.3.7)
endif()

set(GLFW_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(glfw ${EXTERNAL_GLFW_VERSION} EXACT)

if(glfw_FOUND)

  message("Glfw version ${glfw_VERSION} found ...")
  add_custom_target( Glfw COMMAND echo "Glfw build exists, skipping rebuild ...")

else()

  message("Glfw not found! Running external build ...")
  set(GLFW_PREFIX glfw)

  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(GLFW_BUILD_SHARED_LIBS OFF)
  else()
    set(GLFW_BUILD_SHARED_LIBS ON)
  endif()

  ExternalProject_Add ( Glfw
    PREFIX ${EXTERNALS_SOURCE_DIR}/${GLFW_PREFIX}
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG ${EXTERNAL_GLFW_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D BUILD_SHARED_LIBS:BOOL=${GLFW_BUILD_SHARED_LIBS}
    CMAKE_ARGS -D GLFW_BUILD_EXAMPLES:BOOL=OFF
    CMAKE_ARGS -D GLFW_BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D GLFW_BUILD_DOCS:BOOL=OFF
    CMAKE_ARGS -D GLFW_INSTALL:BOOL=ON
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/${GLFW_PREFIX}
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/${GLFW_PREFIX}
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/${GLFW_PREFIX}
  )

endif()

#######################
# TIFF
#######################

if( NOT EXTERNAL_TIFF_VERSION )
  set(EXTERNAL_TIFF_VERSION 4.0.9)
endif()

set(TIFF_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(TIFF ${EXTERNAL_TIFF_VERSION} EXACT)

if(TIFF_FOUND)
  
  message("TIFF version ${TIFF_VERSION} found ...")
  add_custom_target( tiff COMMAND echo "TIFF build exists, skipping rebuild ...")

else()

  message("TIFF not found! Running external build ...")
  set(TIFF_PREFIX tiff)

  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(TIFF_BUILD_SHARED_LIBS OFF)
  else()
    set(TIFF_BUILD_SHARED_LIBS ON)
  endif()

  ExternalProject_Add ( tiff
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tiff-${EXTERNAL_TIFF_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D BUILD_SHARED_LIBS:BOOL=${TIFF_BUILD_SHARED_LIBS}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/${TIFF_PREFIX}
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/${TIFF_PREFIX}
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/${TIFF_PREFIX}
  )

endif()

#######################
# ZMQ
#######################

if( NOT EXTERNAL_ZMQ_VERSION)
  set(EXTERNAL_ZMQ_VERSION 4.3.4)
endif()

set(ZeroMQ_DIR ${EXTERNALS_INSTALL_DIR})
find_package(ZeroMQ ${EXTERNAL_ZMQ_VERSION} EXACT)

if(ZeroMQ_FOUND)

  message("ZMQ version ${ZeroMQ_VERSION} found ...")
  add_custom_target( zmq COMMAND echo "ZMQ build exists, skipping rebuild ...")

else()

  message("ZMQ not found! Running external build ...")
  set(ZMQ_PREFIX zmq)

  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(ZMQ_BUILD_STATIC_LIBS ON)
    set(ZMQ_BUILD_SHARED_LIBS OFF)
  else()
    set(ZMQ_BUILD_STATIC_LIBS OFF)
    set(ZMQ_BUILD_SHARED_LIBS ON)
  endif()

  ExternalProject_Add ( zmq
    PREFIX ${EXTERNALS_SOURCE_DIR}/${ZMQ_PREFIX}
    GIT_REPOSITORY https://github.com/zeromq/libzmq.git
    GIT_TAG v${EXTERNAL_ZMQ_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D BUILD_SHARED:BOOL=${ZMQ_BUILD_SHARED_LIBS}
    CMAKE_ARGS -D BUILD_STATIC:BOOL=${ZMQ_BUILD_STATIC_LIBS}
    CMAKE_ARGS -D WITH_DOCS:BOOL=OFF
    CMAKE_ARGS -D BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/${ZMQ_PREFIX}
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/${ZMQ_PREFIX}
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/${ZMQ_PREFIX}
  )

endif()

#######################
# CPPZMQ
#######################

if( NOT EXTERNAL_CPPZMQ_VERSION )
  set(EXTERNAL_CPPZMQ_VERSION 4.8.1)
endif()

set(CppZMQ_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(CppZMQ ${EXTERNAL_CPPZMQ_VERSION} EXACT)

if(CppZMQ_FOUND)

  message("CppZMQ version ${CppZMQ_VERSION} found ...")
  add_custom_target( cppzmq COMMAND echo "CppZMQ build exists, skipping rebuild ...")

else()

  message("CppZMQ not found! Running external build ...")
  set(CppZMQ_PREFIX cppzmq)

  ExternalProject_Add ( cppzmq
    PREFIX ${EXTERNALS_SOURCE_DIR}/${CppZMQ_PREFIX}
    GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
    GIT_TAG v${EXTERNAL_CPPZMQ_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D CPPZMQ_BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/${CppZMQ_PREFIX}
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/${CppZMQ_PREFIX}
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/${CppZMQ_PREFIX}
  )
  ExternalProject_Add_StepDependencies(cppzmq configure zmq)

endif()

#######################
# Intel SPMD Compiler (ISPC)
#######################

if( NOT EXTERNAL_ISPC_VERSION )
  set(EXTERNAL_ISPC_VERSION 1.17.0)
endif()

set(ISPC_PREFIX ispc)

set(ISPC_ROOT ${EXTERNALS_SOURCE_DIR}/${ISPC_PREFIX})
find_package(ISPC ${EXTERNAL_ISPC_VERSION} EXACT)

message(ISPC exec is ${ISPC_EXECUTABLE})

if(ISPC_EXECUTABLE)

  message("Intel SPMD Compiler version ${ISPC_VERSION} found ...")
  add_custom_target( ispc COMMAND echo "Intel SPMD Compiler found, skipping ...")

else()
  message("Intel SPMD Compiler not found! Downloading ...")

  if(WIN32)
  ExternalProject_Add ( ispc
    PREFIX ${EXTERNALS_SOURCE_DIR}/${ISPC_PREFIX}
    # URL https://github.com/ispc/ispc/releases/download/v1.17.0/ispc-v1.17.0-linux.tar.gz
    URL https://github.com/ispc/ispc/releases/download/v1.17.0/ispc-v1.17.0-windows.zip
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
  )
  else()
  ExternalProject_Add ( ispc
    PREFIX ${EXTERNALS_SOURCE_DIR}/${ISPC_PREFIX}
    URL https://github.com/ispc/ispc/releases/download/v1.17.0/ispc-v1.17.0-linux.tar.gz
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
  )
  endif()

endif()

#######################
# OpenImageDenoise
#######################

if( NOT EXTERNAL_OIDN_VERSION )
  set(EXTERNAL_OIDN_VERSION 1.4.3)
endif()

set(OIDN_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(OIDN ${EXTERNAL_OIDN_VERSION} EXACT)

if(OIDN_FOUND)

  message("OIDN version ${OIDN_VERSION} found ...")
  add_custom_target( OpenImageDenoise COMMAND echo "OpenImageDenoise build exists, skipping rebuild ...")

else()

  message("OpenImageDenoise not found! Running external build ...")
  set(OIDN_PREFIX openimagedenoise)

  if(WIN32)
  if(NOT ISPC_EXECUTABLE)
    find_package(ISPC ${EXTERNAL_ISPC_VERSION} EXACT)
  endif()
  endif()
  get_filename_component(ISPC_EXECUTABLE_DIR ${ISPC_EXECUTABLE} DIRECTORY)
  
  if(LAVA_3RD_LIB_LINK_WITH_STATIC)
    set(OIDN_BUILD_STATIC_LIBS ON)
  else()
    set(OIDN_BUILD_STATIC_LIBS OFF)
  endif()

  set(ISPC_EXECUTABLE "${ISPC_EXECUTABLE_DIR}/ispc")

  ExternalProject_Add ( OpenImageDenoise
    PREFIX ${EXTERNALS_SOURCE_DIR}/${OIDN_PREFIX}
    GIT_REPOSITORY https://github.com/OpenImageDenoise/oidn.git
    GIT_TAG v${EXTERNAL_OIDN_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D ISPC_DIR_HINT=${EXTERNALS_SOURCE_DIR}/${ISPC_PREFIX}/src/ispc/bin
    CMAKE_ARGS -D TBB_ROOT:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D OPENIMAGEIO_ROOT:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D OIDN_STATIC_LIB:BOOL=${OIDN_BUILD_STATIC_LIBS}
    CMAKE_ARGS -D OIDN_API_NAMESPACE:STRING=oidn
    CMAKE_ARGS -D OIDN_FILTER_RT:BOOL=ON
    CMAKE_ARGS -D OIDN_FILTER_RTLIGHTMAP:BOOL=OFF
    CMAKE_ARGS -D OIDN_APPS:BOOL=OFF
    CMAKE_ARGS -D OIDN_APPS_OPENIMAGEIO:BOOL=OFF
    CMAKE_ARGS -D CMAKE_CXX_STANDARD=14
    CMAKE_ARGS -D CMAKE_CXX_FLAGS=-D_GLIBCXX_USE_CXX11_ABI=0
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE}

    TMP_DIR ${EXTERNALS_TMP_DIR}/${OIDN_PREFIX}
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/${OIDN_PREFIX}
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/${OIDN_PREFIX}
  )
  ExternalProject_Add_StepDependencies( OpenImageDenoise configure ispc OpenImageIO Tbb )

endif()

#######################
# NVPro_Core
#######################

set(EXTERNAL_NVPro_Core_VERSION master)

set(NVPro_Core_PREFIX NVPro_Core)

ExternalProject_Add ( NVPro_Core
  PREFIX ${EXTERNALS_SOURCE_DIR}/${NVPro_Core_PREFIX}
  GIT_REPOSITORY https://github.com/nvpro-samples/nvpro_core.git
  GIT_TAG ${EXTERNAL_NVPro_Core_VERSION}
  UPDATE_DISCONNECTED true
  
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""

  INSTALL_COMMAND ${CMAKE_COMMAND}
    -D NVPRO_INST_DIR=${EXTERNALS_INSTALL_DIR}/include/NVPro_Core/
    -D NVPRO_SRC_DIR=<SOURCE_DIR>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/nvpro_install_files.cmake

  TMP_DIR ${EXTERNALS_TMP_DIR}/nvpro_core
  STAMP_DIR ${EXTERNALS_STAMP_DIR}/nvpro_core
)



#######################
# VulkanMemoryAllocator
#######################

set(EXTERNAL_VMA_VERSION master)

set(VMA_PREFIX vulkan_memory_allocator)

ExternalProject_Add ( VMA
  PREFIX ${EXTERNALS_SOURCE_DIR}/${VMA_PREFIX}
  UPDATE_DISCONNECTED true
  
  DOWNLOAD_COMMAND ""

  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND}
    -D VMA_INST_DIR=${EXTERNALS_INSTALL_DIR}/include/VulkanMemoryAllocator/
    -D VMA_SRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/vma
    -P ${CMAKE_CURRENT_SOURCE_DIR}/vma_install_files.cmake

  TMP_DIR ${EXTERNALS_TMP_DIR}/vma
  STAMP_DIR ${EXTERNALS_STAMP_DIR}/vma
)

#######################
# RTXGI
#######################

if( NOT EXTERNAL_RTXGI_VERSION )
  set(EXTERNAL_RTXGI_VERSION 1.2.11)
endif()

set(RTXGI_PREFIX RTXGI)

ExternalProject_Add ( RTXGI
  PREFIX ${EXTERNALS_SOURCE_DIR}/${RTXGI_PREFIX}
  GIT_REPOSITORY https://github.com/NVIDIAGameWorks/RTXGI.git
  GIT_TAG v${EXTERNAL_RTXGI_VERSION}
  UPDATE_DISCONNECTED true
  
  CONFIGURE_COMMAND ""

  BUILD_COMMAND ""

  INSTALL_COMMAND
    ${CMAKE_COMMAND} -E copy_directory    <SOURCE_DIR>/rtxgi-sdk ${EXTERNALS_INSTALL_DIR}/rtxgi-sdk

  TMP_DIR ${EXTERNALS_TMP_DIR}/${RTXGI_PREFIX}
  STAMP_DIR ${EXTERNALS_STAMP_DIR}/${RTXGI_PREFIX}
  BINARY_DIR ${EXTERNALS_BINARY_DIR}/${RTXGI_PREFIX}
)

#######################
# RTXDI
#######################

if( NOT EXTERNAL_RTXDI_VERSION)
  set(EXTERNAL_RTXDI_VERSION 1.3.0)
endif()

set(RTXDI_PREFIX RTXDI)

ExternalProject_Add ( RTXDI
  PREFIX ${EXTERNALS_SOURCE_DIR}/${RTXDI_PREFIX}
  GIT_REPOSITORY https://github.com/NVIDIAGameWorks/RTXDI.git
  GIT_TAG v${EXTERNAL_RTXDI_VERSION}
  UPDATE_DISCONNECTED true
  
  CONFIGURE_COMMAND ""

  BUILD_COMMAND ""

  INSTALL_COMMAND
    COMMAND ${CMAKE_COMMAND} -E copy_directory    <SOURCE_DIR>/rtxdi-sdk ${EXTERNALS_INSTALL_DIR}/rtxdi-sdk
    COMMAND ${CMAKE_COMMAND} -E copy_directory    <SOURCE_DIR>/rtxdi-sdk/include/rtxdi ${EXTERNALS_INSTALL_DIR}/shaders/rtxdi

  TMP_DIR ${EXTERNALS_TMP_DIR}/${RTXDI_PREFIX}
  STAMP_DIR ${EXTERNALS_STAMP_DIR}/${RTXDI_PREFIX}
  BINARY_DIR ${EXTERNALS_BINARY_DIR}/${RTXDI_PREFIX}
)

ExternalProject_Add_StepDependencies(RTXDI configure RTXGI)



#######################
# LZ4 Stream
#######################

set(LZ4_STREAM_PREFIX lz4_stream)
set(LZ4_INSTALL_DIR ${EXTERNALS_INSTALL_DIR}/include/lz4_stream)

if(EXISTS "${LZ4_INSTALL_DIR}/lz4_stream.h")

  message("LZ4_Stream found ...")
  add_custom_target( LZ4_Stream COMMAND echo "LZ4_Stream found, skipping download ...")

else()

  message("Downloading LZ4_Stream ...")
  ExternalProject_Add ( LZ4_Stream
    PREFIX ${EXTERNALS_SOURCE_DIR}/${LZ4_STREAM_PREFIX}
    GIT_REPOSITORY https://github.com/laudrup/lz4_stream.git
    UPDATE_DISCONNECTED true
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND 
      ${CMAKE_COMMAND} -E copy_if_different <SOURCE_DIR>/include/lz4_stream.h ${LZ4_INSTALL_DIR}/lz4_stream.h
  
    TMP_DIR ${EXTERNALS_TMP_DIR}/lz4_stream
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/lz4_stream

  )
endif()

#######################
# NVTT
#######################

set(NVTT_Source ${CMAKE_CURRENT_SOURCE_DIR}/NvidiaTextureTools/linux)

if(EXISTS  ${EXTERNALS_INSTALL_DIR}/lib/libnvtt.so.30106)

  message("NVTT found ...")
  add_custom_target( NVTT COMMAND echo "NVTT found ...")

else()

  ExternalProject_Add ( NVTT
    PREFIX ${EXTERNALS_SOURCE_DIR}/${LZ4_STREAM_PREFIX}
    UPDATE_DISCONNECTED true
    
    DOWNLOAD_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""

    INSTALL_COMMAND 
      COMMAND ${CMAKE_COMMAND} -E copy_directory    ${NVTT_Source}/include/nvtt ${EXTERNALS_INSTALL_DIR}/include/nvtt
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NVTT_Source}/libnvtt.so.30106 ${EXTERNALS_INSTALL_DIR}/lib/libnvtt.so
  
    TMP_DIR ${EXTERNALS_TMP_DIR}/nvtt
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/nvtt

  )
endif()


########################
# FreeImage section
########################

if( NOT EXTERNAL_FREEIMAGE_VERSION)
  set(EXTERNAL_FREEIMAGE_VERSION 3.18.0) 
endif()

set(FreeImage_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(FreeImage ${EXTERNAL_FREEIMAGE_VERSION} EXACT)

if( FreeImage_FOUND )
  add_custom_target( FreeImage COMMAND echo "FreeImage build exists, skipping rebuild ...")
else()

  set(TIFF_ROOT ${EXTERNALS_INSTALL_DIR})
  find_package(TIFF ${EXTERNAL_TIFF_VERSION} EXACT)
  
  if(WIN32)
    # COMMAND msbuild -m FreeImage.2017.sln /property:Configuration=${DEPS_BUILD_TYPE} /property:Platform=x64
    set(FREE_COMMAND "msbuild;-m;FreeImage.2017.sln;/property:Configuration=${DEPS_BUILD_TYPE};/property:Platform=x64")
    # -D SRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/freeimage
    set(FREE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/freeimage)
  else()
    # COMMAND ${MAKE_EXECUTABLE} -f Makefile.gnu
    set(FREE_COMMAND "${MAKE_EXECUTABLE};-f;Makefile.gnu")
    # -D SRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/freeimage/build
    set(FREE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/freeimage/build)
  endif()

  ExternalProject_Add( FreeImage
    PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/freeimage
    UPDATE_DISCONNECTED ON
    LOG_BUILD ON
    USES_TERMINAL ON
    USES_TERMINAL_CONFIGURE ON
    DOWNLOAD_COMMAND ""

    CONFIGURE_COMMAND ""

    BUILD_COMMAND ${FREE_COMMAND}

    INSTALL_COMMAND ${CMAKE_COMMAND}
      -D STATIC_LIB=${LAVA_3RD_LIB_LINK_WITH_STATIC}
      -D SRC_DIR=${FREE_SRC_DIR}
      -D DEST_DIR=${EXTERNALS_INSTALL_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/freeimage_install_files.cmake

    BINARY_DIR ${EXTERNALS_SOURCE_DIR}/../freeimage
    TMP_DIR ${EXTERNALS_TMP_DIR}/freeimage
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/freeimage
  )
  ExternalProject_Add_StepDependencies(FreeImage configure tiff)

endif()

########################
# Slang section
########################

if( NOT EXTERNAL_SLANG_VERSION )
  set(EXTERNAL_SLANG_VERSION 0.24.8) # last tested version was 0.18.6
endif()

set(Slang_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(Slang)

if( Slang_FOUND )
  message("Slang version ${Slang_VERSION} found ...")
  add_custom_target( Slang COMMAND echo "Slang build exists, skipping rebuild ...")
else()

set(SLANG_PREFIX slang)
if(WIN32)
set(SLANG_PLATFORM windows-x64)
else()
set(SLANG_PLATFORM linux-x64)
endif()

if(WIN32)
set(SLANG_BUILD_LIBS "slang slang-glslang gfx")
else()
set(SLANG_BUILD_LIBS "slang slang-glslang")
endif()

set(SLANG_RELEASE_BIN_DIR "bin/${SLANG_PLATFORM}/release")
set(SLANG_DEBUG_BIN_DIR "bin/${SLANG_PLATFORM}/debug")

if(NOT DEFINED SLANG_BUILD_TYPE)
  set( SLANG_BUILD_TYPE CMAKE_BUILD_TYPE)
endif()


if(SLANG_BUILD_TYPE STREQUAL "Release")
  set(SLANG_BUILD_CONFIG release_x64)
  set(SLANG_BIN_DIR ${SLANG_RELEASE_BIN_DIR})
elseif(SLANG_BUILD_TYPE STREQUAL "RelWithDebInfo")
  set(SLANG_BUILD_CONFIG release_x64)
  set(SLANG_BIN_DIR ${SLANG_RELEASE_BIN_DIR})
elseif(SLANG_BUILD_TYPE STREQUAL "MinSizeRel")
  set(SLANG_BUILD_CONFIG release_x64)
  set(SLANG_BIN_DIR ${SLANG_RELEASE_BIN_DIR})
else()
  set(SLANG_BUILD_CONFIG debug_x64)
  set(SLANG_BIN_DIR ${SLANG_DEBUG_BIN_DIR})
endif()

message("Slang build config " ${SLANG_BUILD_CONFIG})

string(REPLACE " " ";" SLANG_BUILD_LIBS_LIST ${SLANG_BUILD_LIBS})

# if(WIN32)
# # -D PREMAKE_URL=https://github.com/shader-slang/slang-binaries/blob/master/premake/premake-5.0.0-alpha16/bin/windows-x64/premake5.exe?raw=true
# set(CONF_PREMAKE_URL "https://github.com/shader-slang/slang-binaries/blob/master/premake/premake-5.0.0-alpha16/bin/windows-x64/premake5.exe?raw=true")
# else()
# # -D PREMAKE_URL=https://github.com/shader-slang/slang-binaries/blob/master/premake/premake-5.0.0-alpha16/bin/linux-64/premake5?raw=true
# set(CONF_PREMAKE_URL "https://github.com/shader-slang/slang-binaries/blob/master/premake/premake-5.0.0-alpha16/bin/linux-64/premake5?raw=true")
# endif()

if(WIN32)
ExternalProject_Add( Slang
  PREFIX ${EXTERNALS_SOURCE_DIR}/${SLANG_PREFIX}
  GIT_REPOSITORY https://github.com/shader-slang/slang.git
  GIT_TAG v${EXTERNAL_SLANG_VERSION}
  UPDATE_DISCONNECTED ON
  BUILD_IN_SOURCE   TRUE
  LOG_BUILD ON
  USES_TERMINAL ON
  USES_TERMINAL_CONFIGURE ON

  CONFIGURE_COMMAND
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/downloads/slang-llvm-13.x-28-win64.zip <SOURCE_DIR>/downloads/slang-llvm-13.x-28-win64.zip
    COMMAND ${CMAKE_COMMAND}
      -D PREMAKE_URL=https://github.com/shader-slang/slang-binaries/blob/master/premake/premake-5.0.0-alpha16/bin/windows-x64/premake5.exe?raw=true
      -D OUTPUT_FILE=<SOURCE_DIR>/premake5.exe
      -P ${CMAKE_CURRENT_SOURCE_DIR}/slang_download_premake.cmake 
    COMMAND ${CMAKE_COMMAND}
      -D GIT_EXECUTABLE=${GIT_EXECUTABLE}
      -D INPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/slang-tag-version.h.in
      -D OUTPUT_FILE=<SOURCE_DIR>/slang-tag-version.h
      -P ${CMAKE_CURRENT_SOURCE_DIR}/slang_generate_version.cmake
    COMMAND ./premake5.exe vs2019 --deps=true --build-glslang=true --enable-embed-stdlib=true --arch=x64
  
  BUILD_COMMAND 
    COMMAND
      # ${MAKE_EXECUTABLE} ${SLANG_BUILD_LIBS_LIST} config=${SLANG_BUILD_CONFIG}
      msbuild -m slang.sln /property:Configuration=${DEPS_BUILD_TYPE} /property:Platform=x64

      # BUILD_COMMAND cmake --build . -j 8 --config Release

  INSTALL_COMMAND ${CMAKE_COMMAND}
    -D SLANG_LIBS_LIST=${SLANG_BUILD_LIBS}
    -D SOURCE_DIR=<SOURCE_DIR>
    -D BIN_DIR=<SOURCE_DIR>/${SLANG_BIN_DIR}/
    -D DEST_DIR=${EXTERNALS_INSTALL_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/slang_install_files.cmake

  TMP_DIR ${EXTERNALS_TMP_DIR}/slang
  STAMP_DIR ${EXTERNALS_STAMP_DIR}/slang
)
else()
ExternalProject_Add( Slang
  PREFIX ${EXTERNALS_SOURCE_DIR}/${SLANG_PREFIX}
  GIT_REPOSITORY https://github.com/shader-slang/slang.git
  GIT_TAG v${EXTERNAL_SLANG_VERSION}
  UPDATE_DISCONNECTED ON
  BUILD_IN_SOURCE   TRUE
  LOG_BUILD ON
  USES_TERMINAL ON
  USES_TERMINAL_CONFIGURE ON

  CONFIGURE_COMMAND
    COMMAND ${CMAKE_COMMAND}
      -D PREMAKE_URL=https://github.com/shader-slang/slang-binaries/blob/master/premake/premake-5.0.0-alpha16/bin/linux-64/premake5?raw=true
      -D OUTPUT_FILE=<SOURCE_DIR>/premake5
      -P ${CMAKE_CURRENT_SOURCE_DIR}/slang_download_premake.cmake 
    COMMAND ${CMAKE_COMMAND}
      -D GIT_EXECUTABLE=${GIT_EXECUTABLE}
      -D INPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/slang-tag-version.h.in
      -D OUTPUT_FILE=<SOURCE_DIR>/slang-tag-version.h
      -P ${CMAKE_CURRENT_SOURCE_DIR}/slang_generate_version.cmake
    COMMAND ./premake5 gmake --deps=true --build-glslang=true --enable-embed-stdlib=true --arch=x64
  
  BUILD_COMMAND 
    #COMMAND ${CMAKE_COMMAND}
    #  -D SOURCE_DIR=<SOURCE_DIR>
    #  -P ${CMAKE_CURRENT_SOURCE_DIR}/slang_patch.cmake
    COMMAND
      ${MAKE_EXECUTABLE} ${SLANG_BUILD_LIBS_LIST} config=${SLANG_BUILD_CONFIG}

  INSTALL_COMMAND ${CMAKE_COMMAND}
    -D SLANG_LIBS_LIST=${SLANG_BUILD_LIBS}
    -D SOURCE_DIR=<SOURCE_DIR>
    -D BIN_DIR=<SOURCE_DIR>/${SLANG_BIN_DIR}/
    -D DEST_DIR=${EXTERNALS_INSTALL_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/slang_install_files.cmake

  TMP_DIR ${EXTERNALS_TMP_DIR}/slang
  STAMP_DIR ${EXTERNALS_STAMP_DIR}/slang
)
endif()

endif()

#######################

add_custom_target( dummy_target COMMAND echo "Third party project dummy target")
add_dependencies( dummy_target 
  zlib
  Tbb
  Blosc
  Boost
  PyBind
  OpenEXR
  Assimp
  OpenVDB
  ispc
  OpenImageDenoise
  OpenImageIO
  RTXDI
  RTXGI
  VMA
  Slang
  Glfw
  zmq
  cppzmq
  tiff
  FreeImage
)
if(WIN32)
  add_dependencies( dummy_target jpeg )
endif()
