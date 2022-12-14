set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH ON)
find_package(PkgConfig)

unset(ISPC_FOUND CACHE)
unset(ISPC_EXECUTABLE CACHE)

set(ISPC_SEARCH_PATHS ${ISPC_ROOT} ${ISPC_ROOT}/bin ${ISPC_ROOT}/src/ispc/bin)

find_program(ISPC_EXECUTABLE
  NAMES 
    ispc
  PATHS 
    ${ISPC_SEARCH_PATHS}
  NO_DEFAULT_PATH
  DOC "Path to the Intel SPMD Compiler (ISPC) executable."      
)

if (NOT ISPC_EXECUTABLE)
  message("Could not find Intel SPMD Compiler (ISPC) (looked in PATH and ${ISPC_ROOT})")
else()
  message(STATUS "Found Intel SPMD Compiler (ISPC): ${ISPC_EXECUTABLE}")
endif()

if(NOT ISPC_VERSION)
  execute_process(COMMAND ${ISPC_EXECUTABLE} --version OUTPUT_VARIABLE ISPC_OUTPUT)
  string(REGEX MATCH " ([0-9]+[.][0-9]+[.][0-9]+)(dev|knl|rc[0-9])? " DUMMY "${ISPC_OUTPUT}")
  set(ISPC_VERSION ${CMAKE_MATCH_1})

  if (ISPC_VERSION VERSION_LESS ISPC_VERSION_REQUIRED)
    message(FATAL_ERROR "Need at least version ${ISPC_VERSION_REQUIRED} of Intel SPMD Compiler (ISPC).")
  else()
    set(ISPC_FOUND ON)
  endif()

  set(ISPC_VERSION ${ISPC_VERSION} CACHE STRING "ISPC Version")
  mark_as_advanced(ISPC_VERSION)
  mark_as_advanced(ISPC_EXECUTABLE)
endif()
