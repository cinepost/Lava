include(CheckIncludeFile)

#add_subdirectory( imgLava )

if(NOT Houdini_DIR)
    set(Houdini_DIR "${HOUDINI_ROOT}/toolkit/cmake")
endif()

find_package( Houdini REQUIRED )
message("Houdini python version: ${_houdini_python_version}")
set(HOUDINI_INST_DIR houdini_${Houdini_VERSION})
message("Houdini install path: ${CMAKE_INSTALL_PREFIX}/${HOUDINI_INST_DIR}")

if(WIN32)
    set(HYTHON_VERSION_MAJOR "3")
    set(HYTHON_VERSION_MINOR "11")
    set(HYTHON_VERSION "${HYTHON_VERSION_MAJOR}.${HYTHON_VERSION_MINOR}")
    message(STATUS "Using python version " ${HYTHON_VERSION} " for Houdini-Lava integration.")
elseif(NOT HOUDINI_ROOT)
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
        OUTPUT "${HDA_FILE}" 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/otls
        COMMAND ${HOTL_EXE} -c ./lava ${HDA_FILE}
        VERBATIM

        # Now add some bad boys to track changes
        DEPENDS ${HDA_SRC_FILES}
    )

    add_custom_target(generate_hda ALL DEPENDS "${HDA_FILE}")

    install(FILES ${HDA_FILE} DESTINATION ${HOUDINI_INST_DIR}/otls)
endif()

# Make lava_version.py used by our SOHO scripts and other python tools
configure_file(lava_version.py.in ${CMAKE_CURRENT_BINARY_DIR}/lava_version.py.install @ONLY)

install(
    PROGRAMS
    	${CMAKE_CURRENT_BINARY_DIR}/lava_version.py.install
    RENAME
    	lava_version.py
    DESTINATION
    	${HOUDINI_INST_DIR}/python${HYTHON_VERSION}libs
)

# Make FBformats used by Houdini
set(DSO_PATH ${CMAKE_INSTALL_PREFIX}/${HOUDINI_INST_DIR}/dso)
configure_file(FBformats.in ${CMAKE_CURRENT_BINARY_DIR}/FBformats.install @ONLY)

install(
    PROGRAMS
        ${CMAKE_CURRENT_BINARY_DIR}/FBformats.install
    RENAME
        FBformats
    DESTINATION
        ${HOUDINI_INST_DIR}
)


# Install shit
install(DIRECTORY scripts DESTINATION ${HOUDINI_INST_DIR})
install(DIRECTORY config DESTINATION ${HOUDINI_INST_DIR})
install(DIRECTORY toolbar DESTINATION ${HOUDINI_INST_DIR})
install(DIRECTORY soho DESTINATION ${HOUDINI_INST_DIR} PATTERN "python" EXCLUDE)
install(DIRECTORY soho/python/ DESTINATION ${HOUDINI_INST_DIR}/soho/python${HYTHON_VERSION})

install(DIRECTORY python/ DESTINATION ${HOUDINI_INST_DIR}/python${HYTHON_VERSION}libs)

install(FILES OPmenu.xml DESTINATION ${HOUDINI_INST_DIR})
install(FILES FBfiles DESTINATION ${HOUDINI_INST_DIR})
install(FILES FBrenderLava DESTINATION ${HOUDINI_INST_DIR})
install(FILES Renderers DESTINATION ${HOUDINI_INST_DIR})
install(FILES LavaTypeFormats DESTINATION ${HOUDINI_INST_DIR})
install(FILES LavaPixelFilterTypes DESTINATION ${HOUDINI_INST_DIR})
install(FILES UsdRenderers.json DESTINATION ${HOUDINI_INST_DIR})