# Locate Houdini's libraries and header files.
# Registers an imported library target named 'Houdini'.

#add_subdirectory( ropLava ) # Houdini render node

install(DIRECTORY otls DESTINATION houdini)
install(DIRECTORY scripts DESTINATION houdini)
install(DIRECTORY config DESTINATION houdini)
install(DIRECTORY soho DESTINATION houdini)
install(DIRECTORY python2.7libs DESTINATION houdini)
install(FILES OPmenu.xml DESTINATION houdini)
install(FILES FBrenderLava DESTINATION houdini)
install(FILES LavaTypeFormats DESTINATION houdini)
install(FILES UsdRenderers.json DESTINATION houdini)