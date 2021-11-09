set(EXTERNALS_BUILD_DIR ${PROJECT_SOURCE_DIR}/build_external/)

file( GLOB SOURCES
    #./*.cpp
    ./Core/*.cpp
    ./Core/API/*.cpp
    ./Core/API/Vulkan/*.cpp
    ./Core/API/Embree/*.cpp
    ./Core/Platform/*.cpp
    ./Core/Platform/Linux/*.cpp
    ./Core/Program/*.cpp
    ./Core/State/*.cpp
    ./Core/BufferTypes/*.cpp
    ./RenderGraph/*.cpp
    ./RenderGraph/BasePasses/*.cpp
    ./RenderPasses/*.cpp
    ./Raytracing/*.cpp
    ./Raytracing/RtProgram/*.cpp
    ./Scene/*.cpp
    ./Scene/Animation/*.cpp
    ./Scene/Camera/*.cpp
    ./Scene/Lights/*.cpp
    ./Scene/Material/*.cpp
    ./Scene/Volume/*.cpp
    ./Scene/Importers/AssimpImporter.cpp
    ./Utils/*.cpp
    ./Utils/Algorithm/*.cpp
    ./Utils/Debug/*.cpp
    ./Utils/Image/*.cpp
    ./Utils/Textures/*.cpp
    ./Utils/Perception/*.cpp
    ./Utils/SampleGenerators/*.cpp
    ./Utils/Sampling/*.cpp
    ./Utils/Sampling/Pseudorandom/*.cpp
    ./Utils/Scripting/*.cpp
    ./Utils/Timing/*.cpp
    ./Experimental/Scene/Lights/*.cpp

    ${PROJECT_SOURCE_DIR}/third_party/mikktspace/mikktspace.c
)

add_library( falcor_lib SHARED ${SOURCES} ${HEADERS} )

# Blosc
set(Blosc_ROOT ${EXTERNALS_BUILD_DIR})
find_package(Blosc 1.21.0 REQUIRED)

# PyBind11
find_package(pybind11 REQUIRED CONFIG)

# Python
find_package( Python3 COMPONENTS Development )
include_directories( ${Python3_INCLUDE_DIRS} )

# Assimp
set(Assimp_ROOT ${EXTERNALS_BUILD_DIR})
find_package(Assimp REQUIRED)

# ZLIB
find_package( ZLIB REQUIRED )
if(ZLIB_FOUND)
    set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
endif()

if(ZLIB_FOUND AND NOT TARGET ZLIB::ZLIB)
  if (CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set( ZLIB_LIBRARY ${ZLIB_LIBRARY_DBG} )
  else()
    set( ZLIB_LIBRARY ${ZLIB_LIBRARY_RELEASE} )
  endif()

  add_library(ZLIB::ZLIB INTERFACE IMPORTED)
  set_target_properties(ZLIB::ZLIB PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${ZLIB_LIBRARY}"
  )
endif()

# Imath
#find_package( Imath REQUIRED CONFIG )

# OpenEXR
set(OpenEXR_ROOT ${EXTERNALS_BUILD_DIR})
find_package( OpenEXR REQUIRED CONFIG )

# OpenImageIO
set(OpenImageIO_ROOT ${EXTERNALS_BUILD_DIR})
find_package( OpenImageIO REQUIRED )
if (OpenImageIO_FOUND)
    message( "OpenImageIO root: ${OpenImageIO_ROOT}")   
    message( "OpenImageIO version: ${OpenImageIO_VERSION}" )
    message( "OpenImageIO libs: ${OPENIMAGEIO_LIBRARIES}")
endif()
include_directories( ${OPENIMAGEIO_INCLUDES} )

# Use FindVulkan module added with CMAKE 3.7
if (NOT CMAKE_VERSION VERSION_LESS 3.7.0)
    message(STATUS "Using module to find Vulkan")
    find_package(Vulkan)
endif()

if (NOT Vulkan_FOUND)
    find_library(Vulkan_LIBRARY NAMES vulkan HINTS "$ENV{VULKAN_SDK}/lib" "${CMAKE_SOURCE_DIR/libs/vulkan}" REQUIRED)
    if (Vulkan_LIBRARY)
        set(Vulkan_FOUND OK)
        message("Using bundled vulkan library version")
    endif()
endif()

# Use the package PkgConfig to detect GTK+ headers/library files
FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

# Setup CMake to use GTK+, tell the compiler where to look for headers
# and to the linker where to look for libraries
include_directories( ${GTK3_INCLUDE_DIRS} )
link_directories( ${GTK3_LIBRARY_DIRS} )

# Add other flags to the compiler
add_definitions(${GTK3_CFLAGS_OTHER})

# FreeImage
find_package( FreeImage REQUIRED )
include_directories( ${FREEIMAGE_INCLUDE_DIRS} )

# TIFF (required by FreeImage )
find_package( TIFF REQUIRED )
include_directories( ${TIFF_INCLUDE_DIRS} )

# Find Boost libs
set(Boost_NO_SYSTEM_PATHS ON)
set(BOOST_ROOT ${EXTERNALS_BUILD_DIR})
find_package( Boost COMPONENTS filesystem system REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )

# We use Boost static libs
add_definitions( -DBOOST_LOG_DYN_LINK )

# SLANG
find_library( SLANG_LIBRARY slang PATHS ${EXTERNALS_BUILD_DIR}/lib )

message ("Using SLANG library: ${SLANG_LIBRARY}")

# OpenVDB/NanoVDB
set(OpenVDB_ROOT ${EXTERNALS_BUILD_DIR})
find_package(OpenVDB 9.0.0 REQUIRED COMPONENTS openvdb)
include_directories( ${OpenVDB_INCLUDE_DIRS})

message("OpenVDB ${OpenVDB_LIBRARY}")
message("OpenVDB inc ${OpenVDB_INCLUDE_DIRS}")


add_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")
add_definitions(-DFALCOR_DLL)

target_link_libraries(
    falcor_lib
    avutil
    avformat
    swscale
    stdc++fs
    Blosc::blosc
    ${ASSIMP_LIBRARIES}
    ${SLANG_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Python3_LIBRARIES}
    ${Vulkan_LIBRARY}
    ${GTK3_LIBRARIES}
    ${FREEIMAGE_LIBRARIES}
    ${TIFF_LIBRARIES}
    ${ZLIB_LIBRARY}
    OpenImageIO::OpenImageIO
    OpenVDB::openvdb
)

# Copy/install all needed shaders
set( SHADERS_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders)
file(MAKE_DIRECTORY ${SHADERS_OUTPUT_DIRECTORY})
file(
    COPY Utils Scene Experimental RenderGraph Testing
    DESTINATION ${SHADERS_OUTPUT_DIRECTORY} 
    FILES_MATCHING PATTERN "*.slan*"
)

# Copy/install falcor data
file(
    COPY Data
    DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} 
)

if(UNIX)
    install( 
        TARGETS falcor_lib
        DESTINATION lib 
    )

    # third party libs
    install(DIRECTORY ${EXTERNALS_BUILD_DIR}/lib/ DESTINATION 3rdparty/lib)

    # Install all needed shaders
    install(
        DIRECTORY Core Experimental Raytracing RenderGraph RenderPasses Scene Testing Utils
        DESTINATION shaders
        FILES_MATCHING PATTERN "*.slan*"
    )

    # Install falcor data
    install(
        DIRECTORY Data/Framework Data/LightProbes Data/NVIDIA
        DESTINATION data
    )
endif()
