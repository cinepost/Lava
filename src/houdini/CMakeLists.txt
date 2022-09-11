# Locate Houdini's libraries and header files.
# Registers an imported library target named 'Houdini'.

#find_package(Git)
#add_custom_target(version
#	${CMAKE_COMMAND} -D SRC=${CURRENT_SOURCE_DIR}/lava_version.py.in
#  -D DST=${CMAKE_BINARY_DIR}/lava_version.py.install
#  -D GIT_EXECUTABLE=${GIT_EXECUTABLE}
#  -P ${PROJECT_SOURCE_DIR}/cmake/GenerateVersionHeader.cmake
#)

configure_file(lava_version.py.in ${CMAKE_CURRENT_BINARY_DIR}/lava_version.py.install @ONLY)

install(
    PROGRAMS
    	${CMAKE_CURRENT_BINARY_DIR}/lava_version.py.install
    RENAME
    	lava_version.py
    DESTINATION
    	houdini/python2.7libs
)

install(DIRECTORY otls DESTINATION houdini)
install(DIRECTORY scripts DESTINATION houdini)
install(DIRECTORY config DESTINATION houdini)
install(DIRECTORY soho DESTINATION houdini)
install(DIRECTORY python2.7libs DESTINATION houdini)
install(FILES OPmenu.xml DESTINATION houdini)
install(FILES FBrenderLava DESTINATION houdini)
install(FILES LavaTypeFormats DESTINATION houdini)
install(FILES UsdRenderers.json DESTINATION houdini)