# Locate Houdini's libraries and header files.
# Registers an imported library target named 'Houdini'.


# Generate our houdini digital asset library
find_program (HOTL_EXE NAMES hotl)

if(NOT HOTL_EXE)
    message(FATAL_ERROR "Unable to find houdini hotl executable! No HDA library will be created and/or installed!")
endif()

set(HDA_FILE ${CMAKE_CURRENT_BINARY_DIR}/lava.hda)

file(GLOB_RECURSE HDA_SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/otls/lava/*
)
    
add_custom_command(
    POST_BUILD
    OUTPUT "${HDA_FILE}" 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/otls
    COMMAND ${HOTL_EXE} -c ./lava ${HDA_FILE}
    VERBATIM

    # Now add some bad boys to track changes
    DEPENDS ${HDA_SRC_FILES}
)

add_custom_target(generate_hda ALL DEPENDS "${HDA_FILE}")

install(FILES ${HDA_FILE} DESTINATION houdini/otls)


# Make lava_version.py used by our SOHO scripts and other python tools
configure_file(lava_version.py.in ${CMAKE_CURRENT_BINARY_DIR}/lava_version.py.install @ONLY)

install(
    PROGRAMS
    	${CMAKE_CURRENT_BINARY_DIR}/lava_version.py.install
    RENAME
    	lava_version.py
    DESTINATION
    	houdini/python2.7libs
)


# Install shit
install(DIRECTORY scripts DESTINATION houdini)
install(DIRECTORY config DESTINATION houdini)
install(DIRECTORY soho DESTINATION houdini)
install(DIRECTORY python2.7libs DESTINATION houdini)
install(FILES OPmenu.xml DESTINATION houdini)
install(FILES FBrenderLava DESTINATION houdini)
install(FILES LavaTypeFormats DESTINATION houdini)
install(FILES LavaPixelFilterTypes DESTINATION houdini)
install(FILES UsdRenderers.json DESTINATION houdini)