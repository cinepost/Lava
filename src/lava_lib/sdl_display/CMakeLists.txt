include(${PROJECT_SOURCE_DIR}/third_party/external_versions.cmake)

file( GLOB SOURCES 
	./*.cpp
	./dspyhlpr.c
) 

include_directories (
	${CMAKE_SOURCE_DIR}/
	${CMAKE_CURRENT_SOURCE_DIR}
)

if (NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set (flags_1 "-Wno-overloaded-virtual")
else()
  set (flags_1 "-MP /wd4251")
endif()


# we will use static libs
add_definitions( -DBOOST_LOG_DYN_LINK )

# Boost
if(WIN32)
set(BOOST_ROOT ${DEPS_DIR})
set(Boost_USE_STATIC_LIBS OFF)
endif()
find_package( Boost ${EXTERNAL_BOOST_VERSION} COMPONENTS log EXACT REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} )

if(WIN32)
add_definitions(-DBOOST_ALL_DYN_LINK)
set(SDL2_INCLUDE_DIRS "${EXTERNALS_INSTALL_DIR}/SDL2-2.0.22/include")
set(SDL2_LIBRARIES "${EXTERNALS_INSTALL_DIR}/SDL2-2.0.22/lib/x64/SDL2.lib")
include_directories(${SDL2_INCLUDE_DIRS})
message("SDL2_LIBRARIES ${SDL2_LIBRARIES}")
else()
set(SDL2_ROOT "${EXTERNALS_INSTALL_DIR}/SDL2-2.0.22")
find_package( SDL2 REQUIRED )
include_directories( ${SDL2_INCLUDE_DIRS} )
endif()

# OpenGL
find_package( OpenGL REQUIRED )

find_library(OpenGL_LIBRARY OpenGL )

if(WIN32)
add_definitions( -DIMGUI_IMPL_OPENGL_LOADER_GLEW )
endif()

# ImGui
add_library(imgui STATIC
    # Main Imgui files
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp

    # SDL2+OpenGL-specific files
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_sdl.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_opengl3.cpp
)

message("OpenGL ${OPENGL_INCLUDE_DIR}")

# Make SDL2 available to Imgui
target_include_directories(imgui PUBLIC 
	${SDL2_INCLUDE_DIRS}
	${OPENGL_INCLUDE_DIR}
)

# imgui/backends/ contains the SDL implementation
target_include_directories(imgui PUBLIC 
	${CMAKE_SOURCE_DIR}/third_party/imgui/
	${CMAKE_SOURCE_DIR}/third_party/imgui/backends/
)

# Configure Imgui to use OpengGL through SDL2
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

# Threads
find_package( Threads )

# GLEW
#set(GLEW_USE_STATIC_LIBS ON)

if(WIN32)
set(GLEW_ROOT "${EXTERNALS_INSTALL_DIR}/glew-2.0.0")
endif()
find_package(GLEW 2.0 REQUIRED)
if(NOT GLEW_FOUND)
  message(WARNING "GLEW NOT found, can't build sdl display driver")
  set(WITH_D_SDL 0)
  return()
endif()

if(WIN32)
include_directories( ${GLEW_INCLUDE_DIRS} )
endif()

message("GLEW ${GLEW_LIBRARIES}")
message("GLEW STATIC ${GLEW_STATIC_LIBRARIES}")
message("GLEW SHARED ${GLEW_SHARED_LIBRARIES}")

add_library ( 
	sdl_display 
	SHARED ${SOURCES} 
)

target_link_libraries( 
	sdl_display
	imgui 
	Boost::log 
	Threads::Threads 
	${OpenGL_LIBRARY}
	${SDL2_LIBRARIES}
	${GLEW_LIBRARIES}
)

if(WIN32)
target_link_libraries( sdl_display
	OpenGL::GL
)
endif()

set_target_properties( sdl_display PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS ON
	PREFIX "d_"
	SUFFIX ".so"
	OUTPUT_NAME "sdl"
)

if(WIN32)
set_target_properties( sdl_display PROPERTIES
	SUFFIX ".dll"
)
endif()


install(
	TARGETS sdl_display
	LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/etc
)

install(
	FILES ${GLEW_LIBRARIS}
	DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)
