include (ExternalProject)

message("Running SuperBuild.cmake")

# Third party external projects
ExternalProject_Add( third_party
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party
  INSTALL_COMMAND ""
  ALWAYS ON
  BUILD_ALWAYS ON
  CONFIGURE_HANDLED_BY_BUILD ON
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/third_party
)

# Main project
ExternalProject_Add( ep_src
  DEPENDS third_party
  SOURCE_DIR ${PROJECT_SOURCE_DIR}
  CMAKE_ARGS -DUSE_SUPERBUILD=OFF
  INSTALL_COMMAND ""
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(ep_src third_party)

add_custom_target(dummy COMMAND echo "Dummy main project target")
add_dependencies(dummy 
  third_party
  ep_src
)