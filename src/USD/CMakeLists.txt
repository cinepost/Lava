#add_subdirectory( hgiVk ) # usd vulkan backend

set(PXR_ENABLE_PTEX_SUPPORT OFF)
set(PXR_ENABLE_PYSIDE_SUPPORT OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/USD/cmake/defaults/")

include(Options)

include(ProjectDefaults)
include(Packages)

include(CXXDefaults)
add_definitions(${_PXR_CXX_DEFINITIONS})
set(CMAKE_CXX_FLAGS "${_PXR_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")

include(Public)

add_subdirectory( hdLava ) # usdview hydra renderer plugin
