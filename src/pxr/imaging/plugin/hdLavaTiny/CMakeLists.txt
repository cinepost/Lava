#set(PXR_PREFIX pxr/imaging)
#set(PXR_PACKAGE hdLavaTiny)
#
#pxr_plugin(${PXR_PACKAGE}
#    LIBRARIES
#        hd
#        tf
#
#    PRIVATE_CLASSES
#        mesh
#        renderDelegate
#        rendererPlugin
#        renderPass
#
#    RESOURCE_FILES
#       plugInfo.json
#
#    DISABLE_PRECOMPILED_HEADERS
#)

add_custom_target(shared_libs)

add_library( hdLavaTiny
  mesh.cpp
  mesh.h
  rendererPlugin.cpp
  rendererPlugin.h
  renderDelegate.cpp
  renderDelegate.h
  renderPass.cpp
  renderPass.h
)

set_property( TARGET hdLavaTiny PROPERTY PREFIX "" )

target_link_libraries( hdLavaTiny
  Houdini
  lava_lib
  lava_utils_lib
  falcor_lib
)

install(
    CODE
    "FILE(WRITE \"${CMAKE_INSTALL_PREFIX}/houdini/dso/usd_plugins/plugInfo.json\"
    \"{
    \\\"Includes\\\": [ \\\"*/resources/\\\" ]
    }\")"
)

set(PLUG_INFO_LIBRARY_PATH "../../usd/hdLavaTiny.so")
set(PLUG_INFO_RESOURCE_PATH "resources")
set(PLUG_INFO_ROOT "..")
configure_file(plugInfo.json.in ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json.install @ONLY)

install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json.install
    RENAME
    plugInfo.json
    DESTINATION
    ${CMAKE_INSTALL_PREFIX}/houdini/dso/usd_plugins/hdLavaTiny/resources/
)

install( TARGETS hdLavaTiny DESTINATION "${CMAKE_INSTALL_PREFIX}/houdini/dso/usd/" )
