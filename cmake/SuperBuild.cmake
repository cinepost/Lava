include (ExternalProject)

message("Running SuperBuild.cmake")

list(APPEND 3RD_ARGS "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

if( DEFINED DEPS_BUILD_TYPE)
  list(APPEND 3RD_ARGS "-DDEPS_BUILD_TYPE=${DEPS_BUILD_TYPE}")
else()
  list(APPEND 3RD_ARGS "-DDEPS_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
endif()

if( DEFINED SLANG_BUILD_TYPE)
  list(APPEND 3RD_ARGS "-DSLANG_BUILD_TYPE=${SLANG_BUILD_TYPE}")
else()
  list(APPEND 3RD_ARGS "-DSLANG_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
endif()

# Third party external projects
ExternalProject_Add( third_party
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party
  INSTALL_COMMAND ""
  ALWAYS ON
  BUILD_ALWAYS ON
  CONFIGURE_HANDLED_BY_BUILD ON
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/third_party
  CMAKE_ARGS ${3RD_ARGS}
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
