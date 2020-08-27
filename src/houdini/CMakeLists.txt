# Locate Houdini's libraries and header files.
# Registers an imported library target named 'Houdini'.

set(HOUDINI_INST "/opt/houdini17.5")
set(HOUDINI_USER "$ENV{HOME}/houdini17.5")

list( APPEND CMAKE_PREFIX_PATH "${HOUDINI_INST}/toolkit/cmake" )


#add_subdirectory( ropLava ) # Houdini render node

install(DIRECTORY soho DESTINATION ${HOUDINI_USER})