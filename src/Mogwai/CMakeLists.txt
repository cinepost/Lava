set (SOURCES
  ./stdafx.cpp
  ./AppData.cpp
  ./Mogwai.cpp
  ./MogwaiScripting.cpp
  ./MogwaiSettings.cpp
)

# ImGui
include_directories(${PROJECT_SOURCE_DIR}/third_party/imgui)

# Python
find_package(PythonLibs 3.7 REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

# Find GLFW
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
include_directories(${GLFW_INCLUDE_DIRS})
link_libraries(${GLFW_LIBRARIES})

# Use FindVulkan module added with CMAKE 3.7
if (NOT CMAKE_VERSION VERSION_LESS 3.7.0)
  message(STATUS "Using module to find Vulkan")
  find_package(Vulkan)
endif()

find_package( Boost COMPONENTS system filesystem REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )
# we will use static libs
add_definitions(-DBOOST_LOG_DYN_LINK)

if (NOT Vulkan_FOUND)
  find_library(Vulkan_LIBRARY NAMES vulkan HINTS "$ENV{VULKAN_SDK}/lib" "${CMAKE_SOURCE_DIR/libs/vulkan}" REQUIRED)
  if (Vulkan_LIBRARY)
    set(Vulkan_FOUND OK)
    message("Using bundled vulkan library version")
  endif()
endif()

# RPATH 
SET(CMAKE_SKIP_BUILD_RPATH  FALSE) # use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) # when building, don't use the install RPATH already (but later on when installing)
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE) # add the automatically determined parts of the RPATH which point to directories outside the build tree to the install RPATH

# Executable 
add_executable ( mogwai ${SOURCES} )
set_target_properties( mogwai PROPERTIES 
  OUTPUT_NAME "mogwai" 
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS ON
)

target_link_libraries( mogwai
  falcor_lib
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_FILESYSTEM_LIBRARY}
  ${PYTHON_LIBRARIES}
  ${Vulkan_LIBRARY}
)

install(TARGETS mogwai DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)