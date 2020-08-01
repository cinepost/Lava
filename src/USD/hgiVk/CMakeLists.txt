file( GLOB SOURCES
    ./*.cpp
)

# USD include dir
include_directories(/opt/USD/include)

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

# Python
find_package( PythonLibs 3.7 REQUIRED )
include_directories( ${PYTHON_INCLUDE_DIRS} )

# FreeImage
find_package(FreeImage REQUIRED)
include_directories( ${FREEIMAGE_INCLUDE_DIRS} )

# Find Boost libs
find_package( Boost COMPONENTS system filesystem REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )

# We use Boost static libs
add_definitions( -DBOOST_LOG_DYN_LINK )

# SLANG
#find_library( SLANG_LIBRARY_DEBUG slang PATHS ${PROJECT_SOURCE_DIR}/third_party/slang/bin/linux-x64/debug )
#find_library( SLANG_LIBRARY_RELEASE slang PATHS ${PROJECT_SOURCE_DIR}/third_party/slang/bin/linux-x64/release )
#set( SLANG_LIBRARY
#  debug ${SLANG_LIBRARY_DEBUG}
#  optimized ${SLANG_LIBRARY_RELEASE}
#)
#message ("Using SLANG library: ${SLANG_LIBRARY}")

add_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")
add_library( hgivk_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    hgivk_lib
    stdc++fs
    #${SLANG_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${PYTHON_LIBRARIES}
    ${Vulkan_LIBRARY}
    ${FREEIMAGE_LIBRARIES}
)

if(APPLE)
    if( COPPER_BUILD_LIBS_AS_FRAMEWORKS )
        #falcor_config_framework( falcor_lib )

    # Framework is called 'hgiVk'
    set_target_properties( hgivk_lib PROPERTIES OUTPUT_NAME "hgiVk" )
    install( TARGETS hgivk_lib DESTINATION "Frameworks" )
  else()
    install( TARGETS hgivk_lib DESTINATION "lib" )
  endif()

elseif(UNIX)
    install( TARGETS hgivk_lib DESTINATION "lib" )
endif()
