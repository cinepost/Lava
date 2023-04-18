set(DEPS_DIR "${EXTERNALS_INSTALL_DIR}/")

include_directories( 
    ${EXTERNALS_INSTALL_DIR}
)

include(${PROJECT_SOURCE_DIR}/third_party/external_versions.cmake)

file( GLOB SOURCES
    ./Core/ErrorHandling.cpp
    ./Core/Framework.cpp
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
    ./Utils/InternalDictionary.cpp
    ./Utils/Cryptomatte/*.cpp
    ./Utils/Algorithm/*.cpp
    ./Utils/Debug/*.cpp
    ./Utils/Image/*.cpp
    ./Utils/Image/TextureDataCacheLRU.cpp
    ./Utils/Textures/*.cpp
    ./Utils/Textures/FilterKernelsLUT.cpp
    ./Utils/Perception/*.cpp
    
    # CPU sample generators
    ./Utils/SampleGenerators/DxSamplePattern.cpp
    ./Utils/SampleGenerators/HaltonSamplePattern.cpp
    ./Utils/SampleGenerators/StratifiedSamplePattern.cpp
    #./Utils/SampleGenerators/PoissonSamplePattern.cpp

    ./Utils/Cryptomatte/*.cpp
    ./Utils/Sampling/*.cpp
    ./Utils/Sampling/Pseudorandom/*.cpp
    ./Utils/Scripting/*.cpp
    ./Utils/Timing/*.cpp
    ./Experimental/Scene/Lights/*.cpp
    ./Experimental/Scene/Materials/*.cpp

    ${PROJECT_SOURCE_DIR}/third_party/mikktspace/mikktspace.c
)

string( TOLOWER ${FALCOR_API_BACKEND} backend_type )
if(backend_type STREQUAL "gfx")
    add_definitions(-D_FLACOR_API_BACKEND_GFX)
    file( GLOB BACKEND_API_SOURCES 
        ./Core/API/GFX/*.cpp
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
set(BLOSC_USE_STATIC_LIBS ON)
set(Blosc_ROOT ${DEPS_DIR})
find_package(Blosc ${EXTERNAL_BLOSC_VERSION} EXACT REQUIRED)

# PyBind11
set(pybind11_ROOT ${DEPS_DIR})
find_package(pybind11 ${EXTERNAL_PYBIND_VERSION} EXACT REQUIRED CONFIG)

# Python
find_package( Python3 REQUIRED COMPONENTS Development )
target_include_directories(falcor_lib PUBLIC ${Python3_INCLUDE_DIRS})

# Assimp
set(assimp_ROOT ${DEPS_DIR})
find_package(assimp ${EXTERNAL_ASSIMP_VERSION} REQUIRED)

# LZ4
find_package( LZ4 REQUIRED )

# ZLIB
find_package( ZLIB REQUIRED )
if(ZLIB_FOUND)
    set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
endif()

if(ZLIB_FOUND AND NOT TARGET ZLIB::ZLIB)
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
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
find_package( OpenEXR ${EXTERNAL_OPENEXR_VERSION} EXACT REQUIRED )

# OpenImageIO
set(OPENIMAGEIO_ROOT ${DEPS_DIR})
find_package( OpenImageIO ${EXTERNAL_OIIO_VERSION} EXACT REQUIRED )
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
set(FreeImage_ROOT ${DEPS_DIR})
find_package( FreeImage ${EXTERNAL_FREEIMAGE_VERSION} EXACT REQUIRED )
include_directories( ${FreeImage_INCLUDE_DIRS} )

# TIFF
set(TIFF_ROOT ${DEPS_DIR})
find_package( TIFF ${EXTERNAL_TIFF_VERSION} EXACT REQUIRED )
include_directories( ${TIFF_INCLUDE_DIRS} )

# Find Boost libs
set(Boost_NO_SYSTEM_PATHS ON)
set(BOOST_ROOT ${DEPS_DIR})
find_package( Boost ${EXTERNAL_BOOST_VERSION} COMPONENTS json filesystem system thread EXACT REQUIRED )
target_include_directories(falcor_lib PUBLIC ${Boost_INCLUDE_DIRS})

# SLANG
set(Slang_ROOT ${DEPS_DIR})
find_library( SLANG_LIBRARY NAMES slang libslang PATHS "${DEPS_DIR}/lib" NO_CMAKE_FIND_ROOT_PATH NO_DEFAULT_PATH)

message ("Using SLANG library: ${SLANG_LIBRARY}")

# OpenVDB/NanoVDB
set(OpenVDB_ROOT ${DEPS_DIR})
find_package(OpenVDB ${EXTERNAL_OPENVDB_VERSION} REQUIRED COMPONENTS openvdb)
include_directories( ${OpenVDB_INCLUDE_DIRS})

# OpenSSL
find_package(OpenSSL REQUIRED)

message("OpenVDB ${OpenVDB_LIBRARY}")
message("OpenVDB include ${OpenVDB_INCLUDE_DIRS}")

# OpenImageDenoiser
set(OIDN_ROOT ${DEPS_DIR})
find_package(OIDN ${EXTERNAL_OIDN_VERSION} EXACT REQUIRED)

# RTXDi/Gi
include_directories( ${DEPS_DIR}/rtxdi-sdk/include )
include_directories( ${DEPS_DIR}/rtxgi-sdk/include )

# NVTT
find_library( NVTT_LIBRARY libnvtt.so PATHS ${DEPS_DIR}/lib )

# GLFW3
set(GLFW_ROOT ${DEPS_DIR})
find_package(GLFW ${EXTERNAL_GLFW_VERSION} EXACT REQUIRED)

add_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")
add_definitions(-DFALCOR_DLL)

if(USE_MEGALIB)
    target_link_libraries(
        falcor_lib
        PUBLIC
            mega_lib
            stdc++fs
    )
else()
    add_definitions( -DBOOST_LOG_DYN_LINK )
    target_link_libraries(
        falcor_lib
        PUBLIC
            stdc++fs
            Blosc::blosc
            ${GLFW_LIBRARY}
            OpenSSL::SSL
            OpenImageIO::OpenImageIO
            OpenVDB::openvdb
            OpenEXR::IlmImf
            ${assimp_LIBRARIES}
            ${SLANG_LIBRARY}
            Boost::system
            Boost::filesystem
            Boost::thread
            Boost::json
            ${Vulkan_LIBRARY}
            ${GTK3_LIBRARIES}
            ${TIFF_LIBRARIES}
            ${ZLIB_LIBRARY}
            ${LZ4_LIBRARY}
            Python3::Python
            FreeImage::FreeImage
            ${OIDN_LIBRARIES}
    )
endif()

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
