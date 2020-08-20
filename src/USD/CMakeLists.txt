#add_subdirectory( hgiVk ) # usd vulkan backend

set(PXR_ENABLE_PTEX_SUPPORT OFF)
set(PXR_ENABLE_PYSIDE_SUPPORT OFF)

list(APPEND CMAKE_MODULE_PATH 
	${CMAKE_SOURCE_DIR}/third_party/USD/cmake
	${CMAKE_SOURCE_DIR}/third_party/USD/cmake/defaults
	${CMAKE_SOURCE_DIR}/third_party/USD/cmake/modules
	${CMAKE_SOURCE_DIR}/third_party/USD/cmake/macros
)

include(Options)

include(ProjectDefaults)
include(Packages)

include(CXXDefaults)
add_definitions(${_PXR_CXX_DEFINITIONS})
set(CMAKE_CXX_FLAGS "${_PXR_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")

include(Public)

#set(CMAKE_CXX_STANDARD 14)

add_subdirectory( hdLava ) # usdview hydra renderer plugin
