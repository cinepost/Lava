cmake_minimum_required(VERSION 3.20)

project("ThirdParty_External")

include(ExternalProject)
include(ProcessorCount)
#ProcessorCount(N)

message("Running third party dependencies build...")

#we need git executable
find_package(Git REQUIRED)

set(N 4)

set( CMAKE_MODULE_PATH 
  ${PROJECT_SOURCE_DIR}/../cmake/modules
)

set(EXTRA_CMAKE_ARGS)
set(LAVA_3RD_LIB_LINK_WITH_STATIC OFF)

set(EXTERNALS_BINARY_DIR ${PROJECT_SOURCE_DIR}/../build/${DEPS_BUILD_TYPE}/external)
set(EXTERNALS_STAMP_DIR ${PROJECT_SOURCE_DIR}/../build/${DEPS_BUILD_TYPE}/external/stamp)
set(EXTERNALS_TMP_DIR ${PROJECT_SOURCE_DIR}/../build/${DEPS_BUILD_TYPE}/external/tmp)

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
set(TBB_ROOT ${EXTERNALS_INSTALL_DIR})
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
    
    BUILD_COMMAND ${MAKE_EXECUTABLE} -j ${N} tbb_build_dir=${EXTERNALS_BINARY_DIR}/tbb/build tbb_build_prefix=${TBB_PREFIX}

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



#######################
# Blosc2
#######################

set(EXTERNAL_BLOSC_VERSION 1.21.0)

set(Blosc_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(Blosc ${EXTERNAL_BLOSC_VERSION} EXACT)

if( Blosc_FOUND )

  message("Blosc version ${EXTERNAL_BLOSC_VERSION} found ...")
  add_custom_target( Blosc COMMAND echo "Blosc build exists, skipping rebuild ...")

else()

  message("Blosc version ${EXTERNAL_BLOSC_VERSION} not found! Running external build ...")
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
    GIT_TAG v${EXTERNAL_BLOSC_VERSION}
    UPDATE_DISCONNECTED true
    
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D BUILD_STATIC:BOOL=${BLOSC_BUILD_STATIC}
    CMAKE_ARGS -D BUILD_SHARED:BOOL=${BLOSC_BUILD_SHARED}
    CMAKE_ARGS -D BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D BUILD_FUZZERS:BOOL=OFF
    CMAKE_ARGS -D BUILD_BENCHMARKS:BOOL=OFF
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE} -j${N}

    TMP_DIR ${EXTERNALS_TMP_DIR}/blosc
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/blosc
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/blosc
  )

endif()


#######################
# Boost
#######################

set(EXTERNAL_BOOST_VERSION 1.73.0)
set(BOOST_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(Boost ${EXTERNAL_BOOST_VERSION} EXACT COMPONENTS filesystem system date_time iostreams program_options)

if(Boost_FOUND)

  message("Boost version ${Boost_VERSION} found ...")
  add_custom_target( Boost COMMAND echo "Boost ${EXTERNAL_BOOST_VERSION} build exists, skipping rebuild ...")

else()
  
  message("Boost version ${EXTERNAL_BOOST_VERSION} not found! Running external build ...")
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
    BUILD_COMMAND ./b2 --build-dir=${EXTERNALS_BINARY_DIR}/boost install link=${BOOST_LINK_STYLE} variant=release threading=multi runtime-link=${BOOST_LINK_STYLE} -j ${N}
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
    INSTALL_DIR ${EXTERNALS_INSTALL_DIR}
    TMP_DIR ${EXTERNALS_TMP_DIR}/boost
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/boost
  )
endif()


#######################
# Pybind11
#######################

set(EXTERNAL_PYBIND_VERSION 2.5.0)

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

    BUILD_COMMAND ${MAKE_EXECUTABLE} -j${N}

    TMP_DIR ${EXTERNALS_TMP_DIR}/pybind
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/pybind
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/pybind
  )

endif()


#######################
# OpenEXR
#######################

set(EXTERNAL_OPENEXR_VERSION 2.5.8)
set(OpenEXR_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(OpenEXR ${EXTERNAL_OPENEXR_VERSION} EXACT)

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
    
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D BUILD_TESTING:BOOL=OFF
    CMAKE_ARGS -D BUILD_SHARED_LIBS:BOOL=${OPENEXR_BUILD_SHARED}
    CMAKE_ARGS -D PYILMBASE_ENABLE:BOOL=OFF
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE} -j${N}

    TMP_DIR ${EXTERNALS_TMP_DIR}/openexr
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/openexr
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/openexr
  )
  ExternalProject_Add_StepDependencies(OpenEXR configure Tbb OpenVDB Blosc)

endif()


#######################
# Assimp
#######################

set(EXTERNAL_ASSIMP_VERSION 5.0.0)
set(assimp_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(assimp ${EXTERNAL_ASSIMP_VERSION} EXACT)

#if(assimp_FOUND)
#  message("Assimp version ${assimp_VERSION} found ...")
#  add_custom_target( Assimp COMMAND echo "Assimp build exists, skipping rebuild ...")
#else()

  message("Assimp version ${EXTERNAL_ASSIMP_VERSION} not found! Running external build ...")
  set(ASSIMP_PREFIX assimp)

  if( LAVA_3RD_LIB_LINK_WITH_STATIC)
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

    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE} -j${N}

    TMP_DIR ${EXTERNALS_TMP_DIR}/assimp
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/assimp
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/assimp
  )
  ExternalProject_Add_StepDependencies(Assimp configure Tbb OpenVDB Blosc)

#endif()


#######################
# OpenVDB
#######################

set(EXTERNAL_OPENVDB_VERSION 9.0.0)

set(OpenVDB_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(OpenVDB ${EXTERNAL_OPENVDB_VERSION} EXACT)

if( OpenVDB_FOUND )

  message("OpenVDB version ${OpenVDB_VERSION} found ...")
  add_custom_target( OpenVDB COMMAND echo "OpenVDB build exists, skipping rebuild ...")

else()

  message("OpenVDB version ${EXTERNAL_OPENVDB_VERSION} not found! Running external build ...")
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
    GIT_TAG v${EXTERNAL_OPENVDB_VERSION}
    
    CMAKE_ARGS -D USE_NANOVDB:BOOL=ON
    CMAKE_ARGS -D USE_BLOSC:BOOL=OFF
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

    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE} -j${N}

    TMP_DIR ${EXTERNALS_TMP_DIR}/openvdb
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/openvdb
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/openvdb
  )
  ExternalProject_Get_property(OpenVDB SOURCE_DIR)
  ExternalProject_Add_StepDependencies(OpenVDB configure Tbb Boost Blosc)

endif()


#######################
# OpenImageIO
#######################

set(EXTERNAL_OIIO_VERSION 2.2.18.0)

set(OpenImageIO_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(OpenImageIO ${EXTERNAL_OIIO_VERSION} EXACT)

if(OpenImageIO_FOUND)

  message("OpenImageIO version ${OPENIMAGEIO_VERSION} found ...")
  add_custom_target( OpenImageIO COMMAND echo "OpenImageIO build exists, skipping rebuild ...")

else()

  message("OpenImageIO not found! Running external build ...")
  set(OIIO_PREFIX openimageio)

  ExternalProject_Add ( OpenImageIO
    PREFIX ${EXTERNALS_SOURCE_DIR}/${OIIO_PREFIX}
    GIT_REPOSITORY https://github.com/OpenImageIO/oiio.git
    GIT_TAG v${EXTERNAL_OIIO_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D CMAKE_CXX_STANDARD=14
    CMAKE_ARGS -D OIIO_BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D OIIO_BUILD_TOOLS:BOOL=OFF
    CMAKE_ARGS -D BUILD_DOCS:BOOL=OFF
    CMAKE_ARGS -D Boost_ROOT=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D TBB_ROOT=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D OpenEXR_ROOT=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D pybind11_ROOT=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS -D USE_PYTHON:BOOL=OFF
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE} -j${N}

    TMP_DIR ${EXTERNALS_TMP_DIR}/oiio
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/oiio
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/oiio
  )
  ExternalProject_Add_StepDependencies(OpenImageIO configure Tbb Blosc PyBind OpenEXR OpenVDB tiff)

endif()


#######################
# GLFW3
#######################

set(EXTERNAL_GLFW_VERSION 3.3.7)

set(GLFW_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(glfw3 ${EXTERNAL_GLFW_VERSION} EXACT)

if(glfw_FOUND)

  message("Glfw version ${glfw_VERSION} found ...")
  add_custom_target( Glfw COMMAND echo "Glfw build exists, skipping rebuild ...")

else()

  message("Glfw not found! Running external build ...")
  set(GLFW_PREFIX glfw)

  ExternalProject_Add ( Glfw
    PREFIX ${EXTERNALS_SOURCE_DIR}/${GLFW_PREFIX}
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG ${EXTERNAL_GLFW_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D BUILD_SHARED_LIBS:BOOL=ON
    CMAKE_ARGS -D GLFW_BUILD_EXAMPLES:BOOL=OFF
    CMAKE_ARGS -D GLFW_BUILD_TESTS:BOOL=OFF
    CMAKE_ARGS -D GLFW_BUILD_DOCS:BOOL=OFF
    CMAKE_ARGS -D GLFW_INSTALL:BOOL=ON
    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE} -j${N}

    TMP_DIR ${EXTERNALS_TMP_DIR}/${GLFW_PREFIX}
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/${GLFW_PREFIX}
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/${GLFW_PREFIX}
  )

endif()

#######################
# TIFF
#######################

set(EXTERNAL_TIFF_VERSION 4.4.0)

set(TIFF_DIR ${EXTERNALS_INSTALL_DIR})
find_package(TIFF ${EXTERNAL_TIFF_VERSION} EXACT)

if(TIFF_FOUND)

  message("TIFF found ...")
  add_custom_target( tiff COMMAND echo "TIFF build exists, skipping rebuild ...")

else()

  message("TIFF not found! Running external build ...")
  set(TIFF_PREFIX tiff)

  ExternalProject_Add ( tiff
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tiff-${EXTERNAL_TIFF_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE} -j${N}

    TMP_DIR ${EXTERNALS_TMP_DIR}/${TIFF_PREFIX}
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/${TIFF_PREFIX}
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/${TIFF_PREFIX}
  )

endif()

#######################
# ZMQ
#######################

set(EXTERNAL_ZMQ_VERSION 4.3.4)

set(ZeroMQ_DIR ${EXTERNALS_INSTALL_DIR})
find_package(ZeroMQ ${EXTERNAL_ZMQ_VERSION} EXACT)

if(ZeroMQ_FOUND)

  message("ZMQ version ${ZeroMQ_VERSION} found ...")
  add_custom_target( zmq COMMAND echo "ZMQ build exists, skipping rebuild ...")

else()

  message("ZMQ not found! Running external build ...")
  set(ZMQ_PREFIX zmq)

  ExternalProject_Add ( zmq
    PREFIX ${EXTERNALS_SOURCE_DIR}/${ZMQ_PREFIX}
    GIT_REPOSITORY https://github.com/zeromq/libzmq.git
    GIT_TAG v${EXTERNAL_ZMQ_VERSION}
    UPDATE_DISCONNECTED true

    CMAKE_ARGS -D CMAKE_BUILD_TYPE=${DEPS_BUILD_TYPE}
    CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_INSTALL_DIR}
    CMAKE_ARGS <SOURCE_DIR>

    BUILD_COMMAND ${MAKE_EXECUTABLE} -j${N}

    TMP_DIR ${EXTERNALS_TMP_DIR}/${ZMQ_PREFIX}
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/${ZMQ_PREFIX}
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/${ZMQ_PREFIX}
  )

endif()

#######################
# CPPZMQ
#######################

set(EXTERNAL_CPPZMQ_VERSION 4.8.1)

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

    BUILD_COMMAND ${MAKE_EXECUTABLE} -j${N}

    TMP_DIR ${EXTERNALS_TMP_DIR}/${CppZMQ_PREFIX}
    STAMP_DIR ${EXTERNALS_STAMP_DIR}/${CppZMQ_PREFIX}
    BINARY_DIR ${EXTERNALS_BINARY_DIR}/${CppZMQ_PREFIX}
  )
  ExternalProject_Add_StepDependencies(cppzmq configure zmq)

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
  #GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  #GIT_TAG ${EXTERNAL_VMA_VERSION}
  UPDATE_DISCONNECTED true
  
  #URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v2.3.0.zip
  #DOWNLOAD_NAME VulkanMemoryAllocator
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

set(EXTERNAL_RTXGI_VERSION 1.2.11)

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

set(EXTERNAL_RTXDI_VERSION 1.3.0)

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
# Slang section
########################

set(Slang_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(Slang)

if( Slang_FOUND )
  message("Slang version ${Slang_VERSION} found ...")
  add_custom_target( Slang COMMAND echo "Slang build exists, skipping rebuild ...")
else()

set(EXTERNAL_SLANG_VERSION 0.24.8) # last tested version was 0.18.6

set(SLANG_PREFIX slang)
set(SLANG_PLATFORM linux-x64)

set(SLANG_BUILD_LIBS "slang slang-glslang")

set(SLANG_RELEASE_BIN_DIR "bin/${SLANG_PLATFORM}/release")
set(SLANG_DEBUG_BIN_DIR "bin/${SLANG_PLATFORM}/debug")

if(NOT DEFINED SLANG_BUILD_TYPE)
  set( SLANG_BUILD_TYPE CMAKE_BUILD_TYPE)
endif()


if (SLANG_BUILD_TYPE STREQUAL "Release")
  set(SLANG_BUILD_CONFIG release_x64)
  set(SLANG_BIN_DIR ${SLANG_RELEASE_BIN_DIR})
else()
  set(SLANG_BUILD_CONFIG debug_x64)
  set(SLANG_BIN_DIR ${SLANG_DEBUG_BIN_DIR})
endif()

message("Slang build config " ${SLANG_BUILD_CONFIG})

string(REPLACE " " ";" SLANG_BUILD_LIBS_LIST ${SLANG_BUILD_LIBS})

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
      ${MAKE_EXECUTABLE} ${SLANG_BUILD_LIBS_LIST} config=${SLANG_BUILD_CONFIG} -j ${N}

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

#######################

add_custom_target( dummy_target COMMAND echo "Third party project dummy target")
add_dependencies( dummy_target 
  Tbb
  Blosc
  Boost
  PyBind
  OpenEXR
  Assimp
  OpenVDB
  OpenImageIO
  RTXDI
  RTXGI
  VMA
  Slang
  Glfw
  zmq
  cppzmq
  tiff
)
