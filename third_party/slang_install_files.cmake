string(REPLACE " " ";" SLANG_BUILD_LIBS_LIST ${SLANG_LIBS_LIST})

foreach (entry IN LISTS SLANG_BUILD_LIBS_LIST)
    set(INPUT_FILE   ${SOURCE_DIR}/lib${entry}.so)
    set(OUTPUT_FILE  ${DEST_DIR}/lib/lib${entry}.so)

    message("Copy ${INPUT_FILE} to ${OUTPUT_FILE} ")
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${INPUT_FILE} ${OUTPUT_FILE})
endforeach()