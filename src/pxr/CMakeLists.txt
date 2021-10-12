#set(USD_ROOT "/opt/USD")

include(Options)

include(ProjectDefaults)

set(PXR_ENABLE_PTEX_SUPPORT OFF)

set(PXR_BUILD_IMAGING OFF)
set(PXR_BUILD_USDVIEW OFF)
#set(PXR_ENABLE_PYTHON_SUPPORT OFF)
#set(PXR_BUILD_DOCUMENTATION OFF)
#set(PXR_BUILD_TESTS OFF)
#set(PXR_VALIDATE_GENERATED_CODE OFF)

include(Packages)

include(CXXDefaults)
add_definitions(${_PXR_CXX_DEFINITIONS})
set(CMAKE_CXX_FLAGS "${_PXR_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")

include(Public)
set(CMAKE_CXX_STANDARD 17)

FIND_PACKAGE( Houdini REQUIRED )

#add_subdirectory( imaging/plugin/hdLava ) # usdview hydra renderer plugin
add_subdirectory( imaging/plugin/hdLavaTiny ) # usdview hydra renderer plugin
