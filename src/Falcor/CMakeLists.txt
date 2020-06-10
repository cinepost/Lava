file( GLOB SOURCES
    ./*.cpp
    ./Core/*.cpp
    ./Core/API/*.cpp
    ./Core/API/Vulkan/*.cpp
    ./Core/Platform/*.cpp
    ./Core/Platform/Linux/*.cpp
    ./Core/Program/*.cpp
    ./Core/State/*.cpp
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
)

set( CMAKE_MODULE_PATH 
    ${CMAKE_SOURCE_DIR}/cmake/modules
)

# ImGui
include_directories(${PROJECT_SOURCE_DIR}/third_party/imgui)
find_package(imgui CONFIG)

# GLFW 
find_package(glfw CONFIG)

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
find_package(PythonLibs 3.7 REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

# FreeImage
find_package(FreeImage REQUIRED)
include_directories(${FREEIMAGE_INCLUDE_DIRS})

# Find Boost libs
find_package( Boost COMPONENTS system filesystem REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )

# We use Boost static libs
add_definitions(-DBOOST_LOG_DYN_LINK)

add_definitions(-D_PROJECT_DIR_="${CMAKE_SOURCE_DIR}")
add_library( falcor_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    falcor_lib
    ${GLFW_LIBRARIES}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${PYTHON_LIBRARIES}
    ${Vulkan_LIBRARY}
    ${GTK3_LIBRARIES}
    ${FREEIMAGE_LIBRARIES}
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
    install( TARGETS falcor_lib DESTINATION "lib" )
endif()
