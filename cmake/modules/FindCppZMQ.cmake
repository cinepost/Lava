set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH ON)
find_package(PkgConfig)

find_path(CppZMQ_INCLUDE_DIR zmq.hpp
  PATHS 
  ${CppZMQ_ROOT}
  ${CppZMQ_ROOT}/include
)


if(EXISTS "${CppZMQ_INCLUDE_DIR}/zmq.hpp")
  set(_CppZMQ_FOUND ON)
else()
  set(_CppZMQ_FOUND OFF)
endif()

if(EXISTS "${CppZMQ_INCLUDE_DIR}/zmq.hpp")
  file( STRINGS ${CppZMQ_INCLUDE_DIR}/zmq.hpp 
    VERSION_MAJOR_STR REGEX "#define[ ]+CPPZMQ_VERSION_MAJOR[ ]+[0-9]+"
  )

  file( STRINGS ${CppZMQ_INCLUDE_DIR}/zmq.hpp 
    VERSION_MINOR_STR REGEX "#define[ ]+CPPZMQ_VERSION_MINOR[ ]+[0-9]+"
  )

  file( STRINGS ${CppZMQ_INCLUDE_DIR}/zmq.hpp 
    VERSION_PATCH_STR REGEX "#define[ ]+CPPZMQ_VERSION_PATCH[ ]+[0-9]+"
  )

  if (VERSION_MAJOR_STR)
    string(REGEX MATCH "[0-9]+" VERSION_MAJOR_STR ${VERSION_MAJOR_STR})
  endif()

  if (VERSION_MINOR_STR)
    string(REGEX MATCH "[0-9]+" VERSION_MINOR_STR ${VERSION_MINOR_STR})
  endif()

  if (VERSION_PATCH_STR)  
    string(REGEX MATCH "[0-9]+" VERSION_PATCH_STR ${VERSION_PATCH_STR})
  endif()

  if (VERSION_MAJOR_STR AND VERSION_MINOR_STR AND VERSION_PATCH_STR) 
    set(CppZMQ_VERSION ${VERSION_MAJOR_STR}.${VERSION_MINOR_STR}.${VERSION_PATCH_STR})
  endif()
endif()

set ( CppZMQ_INCLUDE_DIRS ${CppZMQ_INCLUDE_DIR} )

include ( FindPackageHandleStandardArgs )
# handle the QUIETLY and REQUIRED arguments and set ZMQ_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args ( CppZMQ 
  REQUIRED_VARS CppZMQ_INCLUDE_DIR
  VERSION_VAR CppZMQ_VERSION
)