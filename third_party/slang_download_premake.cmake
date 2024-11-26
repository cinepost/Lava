if(EXISTS ${OUTPUT_FILE})
    message("premake5 already downloaded, skipping ...")
else()

    if(WIN32)
        set(PREMAKE_ARCHIVE ${SOURCE_DIR}/premake5.zip)
    else()
        set(PREMAKE_ARCHIVE ${SOURCE_DIR}/premake5.tar.gz)
    endif()

    find_program( CHMOD_EXECUTABLE
        NAMES chmod
        NAMES_PER_DIR
    )

    file(DOWNLOAD 
        ${PREMAKE_URL}
        ${PREMAKE_ARCHIVE}
    )

    file(ARCHIVE_EXTRACT 
        INPUT ${PREMAKE_ARCHIVE}
        DESTINATION ${SOURCE_DIR}
    )
    
    execute_process(COMMAND ${CHMOD_EXECUTABLE} u+x ${OUTPUT_FILE})
endif()