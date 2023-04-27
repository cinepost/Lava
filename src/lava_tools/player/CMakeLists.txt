set (SOURCES 
	./player.cpp

    # Imgui files
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp

    # Imgui SDL2 and Vulkan specific files
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_sdl.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_vulkan.cpp
)

# Find BOOST
find_package( Boost COMPONENTS filesystem program_options log REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )
# we will use static libs
add_definitions(-DBOOST_LOG_DYN_LINK)

# SDL
find_package( SDL2 REQUIRED )
include_directories( ${SDL2_INCLUDE_DIRS} )

# Imgui (+backends), NVVK
include_directories(
	${CMAKE_SOURCE_DIR}/third_party/imgui/
	${CMAKE_SOURCE_DIR}/third_party/imgui/backends/
    ${PROJECT_SOURCE_DIR}/deps
    ${PROJECT_SOURCE_DIR}/deps/include/VulkanMemoryAllocator
    ${PROJECT_SOURCE_DIR}/deps/include/NVPro_Core
    ${PROJECT_SOURCE_DIR}/deps/include/NVPro_Core/nvp
    ${PROJECT_SOURCE_DIR}/deps/include/NVPro_Core/nvvk
    ${PROJECT_SOURCE_DIR}/deps/include/NVPro_Core/nvh

)

# Executable 
add_executable ( player ${SOURCES} )
set_target_properties( player PROPERTIES OUTPUT_NAME "lplayer-bin" )
 
target_link_libraries( 
	player
	lava_utils_lib
	falcor_lib 
	Boost::log 
	Boost::program_options 
	Boost::filesystem
	${SDL2_LIBRARIES}
)

install(
	TARGETS player 
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
)

install(
	PROGRAMS
		../bin/player
	DESTINATION 
		${CMAKE_INSTALL_PREFIX}/bin/
)
