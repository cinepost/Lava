if(EXISTS ${OUTPUT_FILE})
    message("premake5 already downloaded, skipping ...")
else()
    find_program( CHMOD_EXECUTABLE
        NAMES chmod
        NAMES_PER_DIR
    )

    file(DOWNLOAD 
        ${PREMAKE_URL}
        ${OUTPUT_FILE}
    )
    
    execute_process(COMMAND ${CHMOD_EXECUTABLE} u+x ${OUTPUT_FILE})
endif()