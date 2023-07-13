include(CheckIncludeFile)

if(NOT HOUDINI_ROOT)
    message(AUTHOR WARNING " No HOUDINI_ROOT path specified! Some Houdini tools for Lava will not be built !!!")
else()
    message(STATUS "Houdini root set to " ${HOUDINI_ROOT} ". Finding required components...")
    foreach(_HVAR IN ITEMS HB HH HHP HT)
        execute_process(
            COMMAND bash "-c" "cd ${HOUDINI_ROOT}; source ./houdini_setup &>/dev/null; echo $${_HVAR}"
            COMMAND_ECHO NONE
            OUTPUT_VARIABLE ${_HVAR}
        )
        set(ENV{${_HVAR}} ${${_HVAR}})
        message("Houdini environment variable ${_HVAR} set to " $ENV{${_HVAR}})
    endforeach()

    if(DEFINED ENV{HHP})
        set(HOUDINI_HHP $ENV{HHP})
        message("Houdini python libs dir: " ${HOUDINI_HHP})
    else()
        message(WARNING "Houdini python libs path environment variable not set !")
    endif()

    string(REGEX REPLACE "^.*(python)([0-9]+).*$" "\\2" HYTHON_VERSION_MAJOR "${HOUDINI_HHP}")
    string(REGEX REPLACE "^.*(python)[0-9]+\\.([0-9]+).*$" "\\2" HYTHON_VERSION_MINOR "${HOUDINI_HHP}")

    if(NOT HYTHON_VERSION_MAJOR MATCHES "[23]")
        message( FATAL_ERROR "Invalid Houdini python major version " ${HYTHON_VERSION_MAJOR})
    endif()

    if(NOT HYTHON_VERSION_MINOR MATCHES "[0-9]")
        message( FATAL_ERROR "Invalid Houdini python nimor version " ${HYTHON_VERSION_MINOR})
    endif()

    set(HYTHON_VERSION "${HYTHON_VERSION_MAJOR}.${HYTHON_VERSION_MINOR}")
    message(STATUS "Using python version " ${HYTHON_VERSION} " for Houdini-Lava integration.")
    
endif()



# Generate our houdini digital asset library
find_program (HOTL_EXE NAMES hotl PATHS ${HOUDINI_ROOT}/bin NO_DEFAULT_PATH)

if(NOT HOTL_EXE)
    message(WARNING " Unable to find Houdini 'hotl' executable! No HDA library will be created and/or installed!")
endif()

if(HOTL_EXE)
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
endif()

# Make lava_version.py used by our SOHO scripts and other python tools
configure_file(lava_version.py.in ${CMAKE_CURRENT_BINARY_DIR}/lava_version.py.install @ONLY)

install(
    PROGRAMS
    	${CMAKE_CURRENT_BINARY_DIR}/lava_version.py.install
    RENAME
    	lava_version.py
    DESTINATION
    	houdini/python${HYTHON_VERSION}libs
)


# Install shit
install(DIRECTORY scripts DESTINATION houdini)
install(DIRECTORY config DESTINATION houdini)
install(DIRECTORY toolbar DESTINATION houdini)
install(DIRECTORY soho DESTINATION houdini PATTERN "python" EXCLUDE)
install(DIRECTORY soho/python/ DESTINATION houdini/soho/python${HYTHON_VERSION})

install(DIRECTORY python/ DESTINATION houdini/python${HYTHON_VERSION}libs)

install(FILES OPmenu.xml DESTINATION houdini)
install(FILES FBrenderLava DESTINATION houdini)
install(FILES Renderers DESTINATION houdini)
install(FILES LavaTypeFormats DESTINATION houdini)
install(FILES LavaPixelFilterTypes DESTINATION houdini)
install(FILES UsdRenderers.json DESTINATION houdini)