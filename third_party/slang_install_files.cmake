string(REPLACE " " ";" SLANG_BUILD_LIBS_LIST ${SLANG_LIBS_LIST})

# copy libs
foreach (entry IN LISTS SLANG_BUILD_LIBS_LIST)
    if(WIN32)
        set(INPUT_FILE   ${BIN_DIR}/${entry}.lib)
        set(OUTPUT_FILE  ${DEST_DIR}/lib/${entry}.lib)
        set(INPUT_FILE_DLL   ${BIN_DIR}/${entry}.dll)
        set(OUTPUT_FILE_DLL  ${DEST_DIR}/bin/${entry}.dll)
    else()
    set(INPUT_FILE   ${BIN_DIR}/lib${entry}.so)
    set(OUTPUT_FILE  ${DEST_DIR}/lib/lib${entry}.so)
    endif()

    message("Copy ${INPUT_FILE} to ${OUTPUT_FILE} ")
    if(WIN32)
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${INPUT_FILE_DLL} ${OUTPUT_FILE_DLL})
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${INPUT_FILE} ${OUTPUT_FILE})
endforeach()

# copy headers
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/slang.h ${DEST_DIR}/include/slang/slang.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/slang-com-ptr.h ${DEST_DIR}/include/slang/slang-com-ptr.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/slang-com-helper.h ${DEST_DIR}/include/slang/slang-com-helper.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/slang-tag-version.h ${DEST_DIR}/include/slang/slang-tag-version.h)

#execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/slang-gfx.h ${DEST_DIR}/include/slang/slang-gfx.h)
