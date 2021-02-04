add_subdirectory( Falcor ) # core framework rendering library
add_subdirectory( RenderPasses ) # falcor rendering framework rendering passes plugins

add_subdirectory( lava_utils_lib ) # lava utility library
add_subdirectory( lava_lib ) # rendering library
add_subdirectory( lava_cmd ) # command line renderer

add_subdirectory( houdini ) # SOHO

add_subdirectory( pxr ) # usd hydra renderer plugin
#add_subdirectory( tools ) # falcor tools
#add_subdirectory( Samples ) # falcor samples
#add_subdirectory( Mogwai ) # gui application
