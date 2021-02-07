file( GLOB SOURCES
    ./*.cpp
    ./Core/*.cpp
    ./Core/API/*.cpp
    ./Core/API/Vulkan/*.cpp
    ./Core/Platform/*.cpp
    ./Core/Platform/Linux/*.cpp
    ./Core/Program/*.cpp
    ./Core/State/*.cpp
    ./Core/BufferTypes/*.cpp
    ./RenderGraph/*.cpp
    ./RenderGraph/BasePasses/*.cpp
    ./RenderPasses/*.cpp
    ./Scene/*.cpp
    ./Scene/Animation/*.cpp
    ./Scene/Camera/*.cpp
    ./Scene/Importers/*.cpp
    ./Scene/Lights/*.cpp
    ./Scene/Material/*.cpp
    #./Scene/ParticleSystem/*.cpp
    ./Utils/*.cpp
    ./Utils/Algorithm/*.cpp
    ./Utils/Debug/*.cpp
    ./Utils/Image/*.cpp
    ./Utils/Perception/*.cpp
    ./Utils/SampleGenerators/*.cpp
    ./Utils/Sampling/*.cpp
    ./Utils/Sampling/Pseudorandom/*.cpp
    ./Utils/Scripting/*.cpp
    ./Utils/Timing/*.cpp
    ./Utils/UI/*.cpp
    ./Utils/Video/*.cpp
    ./Experimental/Scene/Lights/*.cpp
    #./Testing/*.cpp

    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_vulkan.cpp
    ${PROJECT_SOURCE_DIR}/src/Externals/dear_imgui_addons/imguinodegrapheditor/imguinodegrapheditor.cpp
    ${PROJECT_SOURCE_DIR}/src/Externals/mikktspace/mikktspace.c
)

add_library( falcor_lib SHARED ${SOURCES} ${HEADERS} )

# PyBind11
find_package(pybind11 REQUIRED CONFIG)

# Python
find_package( Python3 COMPONENTS Development )
include_directories( ${Python3_INCLUDE_DIRS} )

# Assimp
find_package(assimp REQUIRED)
if(assimp_FOUND)
    set(ASSIMP_LIBRARY "assimp")
    add_library(${ASSIMP_LIBRARY} SHARED IMPORTED)
    set_target_properties(${ASSIMP_LIBRARY} PROPERTIES IMPORTED_LOCATION "${ASSIMP_LIBRARY_DIRS}/libassimp.so")
endif(assimp_FOUND)

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
find_package( Imath REQUIRED CONFIG )

# OpenEXR
find_package( OpenEXR REQUIRED CONFIG )

# OpenImageIO
find_package( OpenImageIO REQUIRED CONFIG )
if (OpenImageIO_FOUND)
    message( "OpenImageIO found: ${OpenImageIO_FOUND}" ) 
    message( "OpenImageIO root: ${OpenImageIO_ROOT}")   
    message( "OpenImageIO version: ${OpenImageIO_VERSION}" )
    message( "OpenImageIO libs: ${OPENIMAGEIO_LIBRARIES}")
endif()
include_directories( ${OPENIMAGEIO_INCLUDES} )

# ImGui
include_directories( ${PROJECT_SOURCE_DIR}/third_party/imgui )

# GLFW 
find_package(glfw3 CONFIG REQUIRED)
include_directories(${GLFW_INCLUDE_DIRS})
link_libraries(${GLFW_LIBRARIES})

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
find_package( Boost COMPONENTS system filesystem REQUIRED )
message( "Boost include dirs: " ${Boost_INCLUDE_DIRS} )
include_directories( ${Boost_INCLUDE_DIRS} )

# We use Boost static libs
add_definitions( -DBOOST_LOG_DYN_LINK )

# SLANG
find_library( SLANG_LIBRARY_DEBUG slang PATHS ${PROJECT_SOURCE_DIR}/third_party/slang/bin/linux-x64/debug )

find_library( SLANG_LIBRARY_RELEASE slang PATHS ${PROJECT_SOURCE_DIR}/third_party/slang/bin/linux-x64/release )

set( SLANG_LIBRARY
  debug ${SLANG_LIBRARY_DEBUG}
  optimized ${SLANG_LIBRARY_RELEASE}
)

message ("Using SLANG library: ${SLANG_LIBRARY}")

add_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")
add_definitions(-DFALCOR_DLL)

target_link_libraries(
    falcor_lib

    glfw
    avutil
    avformat
    swscale
    stdc++fs
    ${ASSIMP_LIBRARY}
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
