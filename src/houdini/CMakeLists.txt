# Locate Houdini's libraries and header files.
# Registers an imported library target named 'Houdini'.

#add_subdirectory( ropLava ) # Houdini render node

install(DIRECTORY soho DESTINATION houdini)
install(FILES Renderers DESTINATION houdini)