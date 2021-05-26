# Locate Houdini's libraries and header files.
# Registers an imported library target named 'Houdini'.

#add_subdirectory( ropLava ) # Houdini render node


if (DEFINED ENV{LAVA_DEPLOY})
	# This check is used not to overwrite local development otls (as they also reside in $HOME/houdiniX.XX/otls)
	install(DIRECTORY otls DESTINATION houdini)
endif()

install(DIRECTORY soho DESTINATION houdini)
install(DIRECTORY python2.7libs DESTINATION houdini)
install(FILES OPmenu.xml DESTINATION houdini)
install(FILES FBrenderLava DESTINATION houdini)
install(FILES LavaTypeFormats DESTINATION houdini)
install(FILES UsdRenderers.json DESTINATION houdini)