if ( MSVC )
  ADD_DEFINITIONS(/utf-8)
endif () # MSVC

if(WIN32)
  add_definitions(-DBOOST_LOG_DYN_LINK)
  add_definitions(-DBOOST_USE_WINAPI_VERSION=0x0A00)
endif(WIN32)

add_subdirectory( gfx_lib ) # Patched Slang GFX library

#if(USE_MEGALIB)
#  add_subdirectory( mega_lib )
#endif()

add_subdirectory( Falcor ) # core framework rendering library
add_subdirectory( RenderPasses ) # falcor rendering framework rendering passes plugins

add_subdirectory( lava_utils_lib ) # lava utility library
add_subdirectory( lava_lib ) # lava rendering library

add_subdirectory( lava_cmd ) # lava command line renderer
add_subdirectory( lava_tools ) # lava command line tools

add_subdirectory( houdini ) # SOHO
