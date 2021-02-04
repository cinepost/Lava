#set(USD_ROOT "/opt/USD")

include(Options)

include(ProjectDefaults)

set(PXR_ENABLE_PTEX_SUPPORT OFF)

include(Packages)

include(CXXDefaults)
add_definitions(${_PXR_CXX_DEFINITIONS})
set(CMAKE_CXX_FLAGS "${_PXR_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")

include(Public)
set(CMAKE_CXX_STANDARD 17)

FIND_PACKAGE( Houdini REQUIRED )

add_subdirectory( imaging/plugin/hdLava ) # usdview hydra renderer plugin
