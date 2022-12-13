set(CMAKE_CXX_STANDARD 17)

set (SOURCES 
	./eagle.cpp
	./window.cpp
)

include_directories (
	${CMAKE_SOURCE_DIR}/
	${CMAKE_CURRENT_SOURCE_DIR}
)

# Find BOOST
find_package( Boost COMPONENTS filesystem program_options log REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )
# we will use static libs
add_definitions(-DBOOST_LOG_DYN_LINK)

find_package( SDL2 REQUIRED )
include_directories( ${SDL2_INCLUDE_DIRS} )

# OpenGL
find_package( OpenGL REQUIRED )

find_library(OpenGL_LIBRARY OpenGL )

# ImGui
add_library(_imgui STATIC
    # Main Imgui files
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp

    # SDL2+OpenGL-specific files
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_sdl.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_opengl3.cpp
)

# Make SDL2 available to Imgui
target_include_directories(_imgui PUBLIC 
	${SDL2_INCLUDE_DIRS}
	${OPENGL_INCLUDE_DIR}
)

# imgui/backends/ contains the SDL implementation
target_include_directories(_imgui PUBLIC 
	${CMAKE_SOURCE_DIR}/third_party/imgui/
	${CMAKE_SOURCE_DIR}/third_party/imgui/backends/
)

# Configure Imgui to use OpengGL through SDL2
target_compile_definitions(_imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

# Threads
find_package( Threads )

# ZMQ & CppZMQ
set(ZeroMQ_DIR ${EXTERNALS_INSTALL_DIR})
find_package(ZeroMQ REQUIRED)

set(CppZMQ_ROOT ${EXTERNALS_INSTALL_DIR})
find_package(CppZMQ REQUIRED)

target_include_directories(_imgui PUBLIC 
	${CppZMQ_INCLUDE_DIRS}
	${ZeroMQ_INCLUDE_DIRS}
)

# GLEW
#set(GLEW_USE_STATIC_LIBS ON)

find_package(GLEW 2.0 REQUIRED)
if(NOT GLEW_FOUND)
  message(WARNING "GLEW NOT found, can't build sdl display driver")
  set(WITH_D_SDL 0)
  return()
endif()

# Executable 
add_executable ( eagle ${SOURCES} )
set_target_properties( eagle PROPERTIES 
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS ON
	OUTPUT_NAME "eagle-bin" 
)

target_compile_features(eagle PRIVATE cxx_std_17)


 
target_link_libraries( 
	eagle
	lava_utils_lib
	Boost::log 
	Boost::program_options 
	Boost::filesystem
	_imgui 
	Threads::Threads 
	${OpenGL_LIBRARY} 
	${SDL2_LIBRARIES}
	${GLEW_LIBRARIES}
	${ZeroMQ_LIBRARIES}
)

install(
	TARGETS eagle 
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
)

install(
	PROGRAMS
		../bin/eagle
	DESTINATION 
		${CMAKE_INSTALL_PREFIX}/bin/
)
