set(USD_ROOT "/opt/USD")

include(Options)

include(ProjectDefaults)

set(PXR_ENABLE_PTEX_SUPPORT OFF)

include(Packages)

include(CXXDefaults)
add_definitions(${_PXR_CXX_DEFINITIONS})
set(CMAKE_CXX_FLAGS "${_PXR_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")

include(Public)
set(CMAKE_CXX_STANDARD 17)

FIND_PACKAGE(pxr CONFIG REQUIRED)

message("pxr include dirs: " ${PXR_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${PXR_INCLUDE_DIRS})
LINK_DIRECTORIES(${PXR_LIBRARIES})

add_subdirectory( imaging/plugin/hdLava ) # usdview hydra renderer plugin
