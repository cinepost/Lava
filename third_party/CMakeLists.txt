include(ExternalProject)

set(EXTRA_CMAKE_ARGS)
set(LAVA_3RD_LIB_LINK_WITH_STATIC OFF)

set(EXTERNALS_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/external)
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

#######################
# TBB
#######################

set(TBB_PREFIX tbb)

ExternalProject_Add( Tbb
  PREFIX ${EXTERNALS_SOURCE_DIR}/${TBB_PREFIX}
  GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
  GIT_TAG v2021.1.1
  UPDATE_DISCONNECTED true
  CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_BUILD_DIR}
  CMAKE_ARGS -D TBB_TEST:BOOL=OFF
  CMAKE_ARGS <SOURCE_DIR>
)

set(TBB_ROOT ${EXTERNALS_BUILD_DIR})

#######################
# Blosc2
#######################

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

#######################
# OpenEXR
#######################

set(OPENEXR_PREFIX openexr)

if( LAVA_3RD_LIB_LINK_WITH_STATIC)
  set(OPENEXR_BUILD_SHARED OFF)
else()
  set(OPENEXR_BUILD_SHARED ON)
endif()

ExternalProject_Add( OpenEXR
  PREFIX ${EXTERNALS_SOURCE_DIR}/${OPENEXR_PREFIX}
  GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openexr.git
  GIT_TAG v3.0.5
  UPDATE_DISCONNECTED true
  CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_BUILD_DIR}
  CMAKE_ARGS -D BUILD_TESTING:BOOL=OFF
  CMAKE_ARGS -D BUILD_SHARED_LIBS:BOOL=${OPENEXR_BUILD_SHARED}
  CMAKE_ARGS -D PYILMBASE_ENABLE:BOOL=OFF
  CMAKE_ARGS <SOURCE_DIR>
)

#######################
# Pybind11
#######################

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

#######################
# Boost
#######################

set(EXTERNAL_BOOST_VERSION 1.73.0)
set(BOOST_PREFIX boost)
set(BOOST_ROOT ${EXTERNALS_BUILD_DIR})

if( LAVA_3RD_LIB_LINK_WITH_STATIC)
  set(BOOST_LINK_STYLE static)
else()
  set(BOOST_LINK_STYLE shared)
endif()

find_package(Boost ${EXTERNAL_BOOST_VERSION} EXACT COMPONENTS filesystem system date_time iostreams program_options)

if(Boost_FOUND)
  add_custom_target( Boost COMMAND echo "Boost ${EXTERNAL_BOOST_VERSION} build exists, skipping rebuild ...")
else()
  message("BOOST NOT FOUND !!!!!!!!!!!!!!!!!!!!!")
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
# Assimp
#######################

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


#######################
# OpenVDB
#######################

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
add_dependencies(OpenVDB Tbb Blosc Boost)


#######################
# FreeImage
#######################

set(FREEIMAGE_PREFIX freeimage)

ExternalProject_Add ( FreeImage
  PREFIX ${EXTERNALS_SOURCE_DIR}/${FREEIMAGE_PREFIX}
  GIT_REPOSITORY https://github.com/WinMerge/freeimage.git
  #URL http://downloads.sourceforge.net/freeimage/FreeImage3180.zip
  BUILD_IN_SOURCE 1
  UPDATE_DISCONNECTED true
  CONFIGURE_COMMAND ${MAKE_EXECUTABLE} 
  INSTALL_COMMAND ""
)

#######################
# OpenImageIO
#######################

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
  CMAKE_ARGS -D OpenEXR_ROOT=${EXTERNALS_BUILD_DIR}
  CMAKE_ARGS -D pybind11_ROOT=${EXTERNALS_BUILD_DIR}
  CMAKE_ARGS -D CMAKE_INSTALL_PREFIX:PATH=${EXTERNALS_BUILD_DIR}
  CMAKE_ARGS <SOURCE_DIR>
)
add_dependencies(OpenImageIO PyBind OpenEXR OpenVDB FreeImage)

#########################
# Vulkan Memory Allocator
#########################

set(VMA_PREFIX vulkan_memory_allocator)

ExternalProject_Add ( VulkanMemoryAllocator
  PREFIX ${EXTERNALS_SOURCE_DIR}/${VMA_PREFIX}
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG v2.3.0
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND}
    -D VMA_SRC_DIR=<SOURCE_DIR>/src
    -D VMA_INST_DIR=${EXTERNALS_BUILD_DIR}/include/VulkanMemoryAllocator
    -P ${CMAKE_CURRENT_SOURCE_DIR}/vma_install_files.cmake
  LOG_DOWNLOAD ON
)
ExternalProject_Get_Property(VulkanMemoryAllocator source_dir)
set(VulkanMemoryAllocator_INCLUDE_DIR ${source_dir}/src)

########################
# Slang section
########################

set(SLANG_VER 0.19.14) # last working version 0.18.6
set(SLANG_PREFIX slang)
set(SLANG_PLATFORM linux-x64)
set(SLANG_BUILD_LIBS "slang slang-glslang")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(SLANG_BUILD_CONFIG release_x64)
  set(SLANG_BIN_DIR "bin/${SLANG_PLATFORM}/release")
else()
  set(SLANG_BUILD_CONFIG debug_x64)
  set(SLANG_BIN_DIR "bin/${SLANG_PLATFORM}/debug")
endif()

string(REPLACE " " ";" SLANG_BUILD_LIBS_LIST ${SLANG_BUILD_LIBS})

ExternalProject_Add( Slang
  PREFIX ${EXTERNALS_SOURCE_DIR}/${SLANG_PREFIX}
  GIT_REPOSITORY https://github.com/shader-slang/slang.git
  GIT_REMOTE_NAME releases/tags/v${SLANG_VER}
  UPDATE_DISCONNECTED ON
  BUILD_IN_SOURCE   TRUE
  EXCLUDE_FROM_ALL  TRUE
  BUILD_ALWAYS      TRUE
  LOG_BUILD ON
  USES_TERMINAL ON
  USES_TERMINAL_CONFIGURE ON

  CONFIGURE_COMMAND 
    COMMAND ${CMAKE_COMMAND}
      -D GIT_EXECUTABLE=${GIT_EXECUTABLE}
      -D INPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/slang-tag-version.h.in
      -D OUTPUT_FILE=<SOURCE_DIR>/slang-tag-version.h
      -P ${CMAKE_CURRENT_SOURCE_DIR}/slang_generate_version.cmake
    COMMAND ./premake5 gmake --build-glslang=true --enable-embed-stdlib=true --arch=x64
  
  BUILD_COMMAND ${MAKE_EXECUTABLE} ${SLANG_BUILD_LIBS_LIST} config=${SLANG_BUILD_CONFIG}
  INSTALL_COMMAND ${CMAKE_COMMAND}
    -D SLANG_LIBS_LIST=${SLANG_BUILD_LIBS}
    -D SOURCE_DIR=<SOURCE_DIR>/${SLANG_BIN_DIR}/
    -D DEST_DIR=${EXTERNALS_BUILD_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/slang_install_files.cmake

  BUILD_BYPRODUCTS "<SOURCE_DIR>/${SLANG_BIN_DIR}/libslang.so"
  BUILD_BYPRODUCTS "<SOURCE_DIR>/${SLANG_BIN_DIR}/libslang-glslang.so"
)

ExternalProject_Get_Property(Slang SOURCE_DIR)
set(Slang_SOURCE_DIR ${SOURCE_DIR})

set(SLANG_LIBRARY ${Slang_SOURCE_DIR}/${SLANG_BIN_DIR}/libslang.so)
add_library(slang SHARED IMPORTED GLOBAL)
set_property(
    TARGET slang
    PROPERTY
        IMPORTED_LOCATION "${SLANG_LIBRARY}"
)

message("Slang_lib_file ${SLANG_LIBRARY}")
message("Slang ${slang}")
add_dependencies(slang SLANG_LIBRARY)

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

#####################
#### 3rd megalib ####
#####################

#if( LAVA_3RD_LIB_LINK_WITH_STATIC)
#  file(WRITE null.cpp "")
#  add_library(lava_3rd_lib SHARED null.cpp) 
#else()
#  add_library(lava_3rd_lib INTERFACE) 
#endif()

#add_dependencies(lava_3rd_lib 
#  Tbb
#  Blosc
#  PyBind
#  Boost
#  OpenEXR
#  Assimp
#  OpenImageIO
#  OpenVDB
#  Slang
#  VulkanMemoryAllocator
#)

#target_link_libraries(
#  lava_3rd_lib INTERFACE
#  ${Boost_LIBRARIES}
#  Assimp
#  #$<TARGET_OBJECTS:Tbb>
#  #$<TARGET_OBJECTS:blosc2_static>
#  #$<TARGET_OBJECTS:Boost>
#  #$<TARGET_OBJECTS:OpenEXR>
#  #$<TARGET_OBJECTS:OpenImageIO>
#)


#if(UNIX)
#  install( TARGETS lava_3rd_lib DESTINATION "lib" )
#endif()
