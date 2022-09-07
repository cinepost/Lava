set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set (SOURCES 
	./ltxview.cpp

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

# RPATH 
SET(CMAKE_SKIP_BUILD_RPATH  FALSE) # use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) # when building, don't use the install RPATH already (but later on when installing)
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE) # add the automatically determined parts of the RPATH which point to directories outside the build tree to the install RPATH

# Executable 
add_executable ( ltxview ${SOURCES} )
set_target_properties( ltxview PROPERTIES OUTPUT_NAME "ltxview-bin" )
 
target_link_libraries( 
	ltxview
	lava_utils_lib
	falcor_lib 
	Boost::log 
	Boost::program_options 
	Boost::filesystem
	${SDL2_LIBRARIES}
)

install(
	TARGETS ltxview 
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
)

install(
	PROGRAMS
		../bin/ltxview
	DESTINATION 
		${CMAKE_INSTALL_PREFIX}/bin/
)
