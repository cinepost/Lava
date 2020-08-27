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
    ./Scene/ParticleSystem/*.cpp
    ./Utils/*.cpp
    ./Utils/Algorithm/*.cpp
    ./Utils/Debug/*.cpp
    ./Utils/Image/Bitmap.cpp
    ./Utils/Perception/*.cpp
    ./Utils/SampleGenerators/*.cpp
    ./Utils/Sampling/*.cpp
    ./Utils/Sampling/Pseudorandom/*.cpp
    ./Utils/Scripting/*.cpp
    ./Utils/Timing/*.cpp
    ./Utils/UI/*.cpp
    ./Utils/Video/*.cpp
    ./Experimental/Scene/Lights/*.cpp
    ./Testing/*.cpp

    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/examples/imgui_impl_glfw.cpp
    ${PROJECT_SOURCE_DIR}/third_party/imgui/examples/imgui_impl_vulkan.cpp
    ${PROJECT_SOURCE_DIR}/src/Externals/dear_imgui_addons/imguinodegrapheditor/imguinodegrapheditor.cpp
    ${PROJECT_SOURCE_DIR}/src/Externals/mikktspace/mikktspace.c
)

# ImGui
include_directories(${PROJECT_SOURCE_DIR}/third_party/imgui)

# Assimp (built within project)
#set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_BINARY_DIR}/third_party/assimp)# ASSIMP search path
#find_package(assimp CONFIG REQUIRED)
#include_directories(${assimp_INCLUDE_DIRS})
#if (assimp_FOUND)
#    message("Assimp inc: ${ASSIMP_INCLUDE_DIR}" )
#endif()

# set any variables you might need to set for your app and assimp
set(BUILD_ASSIMP_TOOLS  ON)
set(ASSIMP_BUILD_STATIC_LIB ON)

# add assimp source dir as a subdirectory, effectively making 
# assimp's CMakeLists.txt part of your build
add_subdirectory( ${PROJECT_SOURCE_DIR}/third_party/assimp ${CMAKE_BINARY_DIR}/assimp)


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
INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})

# Add other flags to the compiler
ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})


# Python
find_package( PythonLibs 3.7 REQUIRED )
include_directories( ${PYTHON_INCLUDE_DIRS} )

# FreeImage
find_package(FreeImage REQUIRED)
include_directories( ${FREEIMAGE_INCLUDE_DIRS} )

# TIFF (required by FreeImage )
find_package( TIFF REQUIRED )
include_directories( ${TIFF_INCLUDE_DIRS} )

# Find Boost libs
find_package( Boost COMPONENTS system filesystem REQUIRED )
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
add_library( falcor_lib SHARED ${SOURCES} ${HEADERS} )
#add_dependencies( falcor_lib assimp )

target_link_libraries(
    falcor_lib
    assimp
    glfw
    avutil
    avformat
    swscale
    stdc++fs
    ${SLANG_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${PYTHON_LIBRARIES}
    ${Vulkan_LIBRARY}
    ${GTK3_LIBRARIES}
    ${FREEIMAGE_LIBRARIES}
    ${TIFF_LIBRARIES}
)

# Copy/install all needed shaders
set( SHADERS_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders)
file(MAKE_DIRECTORY ${SHADERS_OUTPUT_DIRECTORY})
file(
    COPY Utils Scene Experimental RenderGraph Testing
    DESTINATION ${SHADERS_OUTPUT_DIRECTORY} 
    FILES_MATCHING
        PATTERN "*.slang*"
)

# Copy/install falcor data
file(
    COPY Data
    DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} 
)

if(APPLE)
    if( COPPER_BUILD_LIBS_AS_FRAMEWORKS )
	   #falcor_config_framework( falcor_lib )

        # Framework is called 'Falcor'
        set_target_properties( falcor_lib PROPERTIES OUTPUT_NAME "Falcor" )
        install( TARGETS falcor_lib DESTINATION "Frameworks" )
    else()
        install( TARGETS falcor_lib DESTINATION "lib" )
    endif()

elseif(UNIX)
    install( TARGETS falcor_lib DESTINATION lib )
endif()
