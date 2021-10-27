string(REPLACE " " ";" SLANG_BUILD_LIBS_LIST ${SLANG_LIBS_LIST})


message("SLANG_BUILD_LIBS_LIST ${SLANG_BUILD_LIBS_LIST}")

foreach (entry IN LISTS SLANG_BUILD_LIBS_LIST)
    set(INPUT_FILE   ${SOURCE_DIR}/lib${entry}.so)
    set(OUTPUT_FILE  ${DEST_DIR}/lib/lib${entry}.so)

    message("Copy ${INPUT_FILE} to ${OUTPUT_FILE} ")

    #if(EXISTS ${OUTPUT_FILE})
    #    message("Slang project ${OUTPUT_FILE} already installed, skipping ...")
    #else()
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${INPUT_FILE} ${OUTPUT_FILE})
    #endif()
endforeach()