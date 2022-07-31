
message( "GFX LIB " ${FALCOR_API_BACKEND} )

string( TOLOWER ${FALCOR_API_BACKEND} backend_type )
if(backend_type STREQUAL "gfx")
	add_subdirectory( gfx_lib ) # Patched Slang GFX library
endif()

add_subdirectory( Falcor ) # core framework rendering library
add_subdirectory( RenderPasses ) # falcor rendering framework rendering passes plugins

add_subdirectory( lava_utils_lib ) # lava utility library
add_subdirectory( lava_lib ) # lava rendering library
add_subdirectory( lava_cmd ) # lava command line renderer

#add_subdirectory( lava_tools ) # lava command line tools

add_subdirectory( houdini ) # SOHO

#add_subdirectory( pxr ) # usd hydra renderer plugin
