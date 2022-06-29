string(REPLACE " " ";" SLANG_BUILD_LIBS_LIST ${SLANG_LIBS_LIST})

# copy libs
foreach (entry IN LISTS SLANG_BUILD_LIBS_LIST)
    set(INPUT_FILE   ${BIN_DIR}/lib${entry}.so)
    set(OUTPUT_FILE  ${DEST_DIR}/lib/lib${entry}.so)

    message("Copy ${INPUT_FILE} to ${OUTPUT_FILE} ")
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${INPUT_FILE} ${OUTPUT_FILE})
endforeach()

# copy headers
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/slang.h ${DEST_DIR}/include/slang/slang.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/slang-gfx.h ${DEST_DIR}/include/slang/slang-gfx.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/slang-com-ptr.h ${DEST_DIR}/include/slang/slang-com-ptr.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/slang-com-helper.h ${DEST_DIR}/include/slang/slang-com-helper.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/slang-tag-version.h ${DEST_DIR}/include/slang/slang-tag-version.h)