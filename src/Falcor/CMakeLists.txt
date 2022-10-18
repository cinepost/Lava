set(DEPS_DIR "${EXTERNALS_INSTALL_DIR}/")

set(BACKEND_TYPE Vulkan)

include_directories( 
    ${EXTERNALS_INSTALL_DIR}
)

file( GLOB SOURCES
    ./Core/*.cpp
    ./Core/API/*.cpp
    ./Core/API/BlitToBufferContext.cpp
    ./Core/Platform/*.cpp
    ./Core/Platform/Linux/Linux.cpp
    ./Core/Program/*.cpp
    ./Core/State/*.cpp
    ./Core/BufferTypes/*.cpp
    ./RenderGraph/*.cpp
    ./RenderGraph/BasePasses/*.cpp
    ./RenderPasses/*.cpp
    ./Rendering/RTXDI/*.cpp
    #./Rendering/RTXGI/*.cpp
    ./Scene/*.cpp
    ./Scene/Animation/*.cpp
    ./Scene/Camera/*.cpp
    ./Scene/Lights/*.cpp
    ./Scene/Material/*.cpp
    ./Scene/MaterialX/*.cpp
    ./Scene/Volume/*.cpp
    ./Scene/Importers/AssimpImporter.cpp
    ./Utils/*.cpp
    ./Utils/Cryptomatte/*.cpp
    ./Utils/Algorithm/*.cpp
    ./Utils/Debug/*.cpp
    ./Utils/Image/*.cpp
    ./Utils/Image/TextureDataCacheLRU.cpp
    ./Utils/Textures/*.cpp
    ./Utils/Perception/*.cpp
    ./Utils/SampleGenerators/*.cpp
    ./Utils/Sampling/*.cpp
    ./Utils/Sampling/Pseudorandom/*.cpp
    ./Utils/Scripting/*.cpp
    ./Utils/Timing/*.cpp
    ./Experimental/Scene/Lights/*.cpp
    ./Experimental/Scene/Material/*.cpp

    ${PROJECT_SOURCE_DIR}/third_party/mikktspace/mikktspace.c
)

string( TOLOWER ${FALCOR_API_BACKEND} backend_type )
if(backend_type STREQUAL "vulkan")
    file( GLOB BACKEND_API_SOURCES
        ./Core/API/Vulkan/*.cpp
    )
elseif(backend_type STREQUAL "gfx")
    add_definitions(-D_FLACOR_API_BACKEND_GFX)
    file( GLOB BACKEND_API_SOURCES 
        ./Core/API/GFX/*.cpp
        ./Core/API/GFX_LIB/*.cpp
        ./Core/API/GFX_LIB/vulkan/*.cpp
    )
else()
     message( FATAL_ERROR "Unsupported backend type specified !!!" )
endif()
list (APPEND SOURCES ${BACKEND_API_SOURCES})

add_library( falcor_lib SHARED ${SOURCES} ${HEADERS} )

if(MSVC)
    target_compile_options(falcor_lib PUBLIC /arch:SSE4.1)
else()
    target_compile_options(falcor_lib PUBLIC -march=native -msse4.1)
endif()

# Blosc
set(Blosc_ROOT ${DEPS_DIR})
find_package(Blosc 1.21.0 REQUIRED)

# PyBind11
find_package(pybind11 REQUIRED CONFIG)

# Python
find_package( Python3 REQUIRED COMPONENTS Development )
target_include_directories(falcor_lib PUBLIC ${Python3_INCLUDE_DIRS})

# Assimp
set(assimp_ROOT ${DEPS_DIR})
find_package(assimp 5.0.0 REQUIRED)

# LZ4
find_package( LZ4 REQUIRED )

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

# OpenEXR
set(OpenEXR_ROOT ${DEPS_DIR})
find_package( OpenEXR 2.5 REQUIRED )

# OpenImageIO
set(OpenImageIO_ROOT ${DEPS_DIR})
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

# FreeImage
find_package( FreeImage REQUIRED )
include_directories( ${FREEIMAGE_INCLUDE_DIRS} )

# TIFF (required by FreeImage )
find_package( TIFF REQUIRED )
include_directories( ${TIFF_INCLUDE_DIRS} )

# Find Boost libs
set(Boost_NO_SYSTEM_PATHS ON)
set(BOOST_ROOT ${DEPS_DIR})
find_package( Boost 1.73.0 COMPONENTS filesystem system thread REQUIRED )
target_include_directories(falcor_lib PUBLIC ${Boost_INCLUDE_DIRS})

# We use Boost static libs
add_definitions( -DBOOST_LOG_DYN_LINK )

# SLANG
set(Slang_ROOT ${DEPS_DIR})
#set(SLANG_LIBRARY "")
find_library( SLANG_LIBRARY NAMES slang libslang PATHS "${DEPS_DIR}/lib" NO_CMAKE_FIND_ROOT_PATH NO_DEFAULT_PATH)

message ("Using SLANG library: ${SLANG_LIBRARY}")
message ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
message ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")

# OpenVDB/NanoVDB
set(OpenVDB_ROOT ${DEPS_DIR})
find_package(OpenVDB 9.0.0 REQUIRED COMPONENTS openvdb)
include_directories( ${OpenVDB_INCLUDE_DIRS})

# OpenSSL
find_package(OpenSSL REQUIRED)

message("OpenVDB ${OpenVDB_LIBRARY}")
message("OpenVDB inc ${OpenVDB_INCLUDE_DIRS}")

# RTXDi/Gi
include_directories( ${DEPS_DIR}/rtxdi-sdk/include )
include_directories( ${DEPS_DIR}/rtxgi-sdk/include )

# NVTT
find_library( NVTT_LIBRARY libnvtt.so PATHS ${DEPS_DIR}/lib )

# GLFW3
set(GLFW_ROOT ${DEPS_DIR})
find_package(glfw3 3.3 REQUIRED)

add_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")
add_definitions(-DFALCOR_DLL)

target_link_libraries(
    falcor_lib
    PUBLIC
        stdc++fs
        Blosc::blosc
        glfw3
        OpenSSL::SSL
        OpenImageIO::OpenImageIO
        OpenVDB::openvdb
        ${assimp_LIBRARIES}
        ${SLANG_LIBRARY}
        Boost::system
        Boost::filesystem
        Boost::thread
        ${Vulkan_LIBRARY}
        ${GTK3_LIBRARIES}
        ${FREEIMAGE_LIBRARIES}
        ${TIFF_LIBRARIES}
        ${ZLIB_LIBRARY}
        ${LZ4_LIBRARY}
        Python3::Python
)

if(backend_type STREQUAL "gfx")
target_link_libraries(
    falcor_lib
    PUBLIC
        gfx_lib
)
endif()

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
        RUNTIME_DEPENDENCY_SET falcor_deps
    )

    # Install all needed shaders
    install(
        DIRECTORY Core Experimental Raytracing RenderGraph Rendering RenderPasses Scene Testing Utils
        DESTINATION shaders
        FILES_MATCHING PATTERN "*.slan*"
    )

    # Install falcor data
    install(
        DIRECTORY Data/Framework Data/LightProbes Data/NVIDIA
        DESTINATION data
    )
endif()
