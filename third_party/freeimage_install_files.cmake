execute_process( COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC_DIR}/libfreeimage-3.18.0.so ${DEST_DIR}/lib/libfreeimage-3.18.0.so )
execute_process( COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC_DIR}/libfreeimage.a ${DEST_DIR}/lib/libfreeimage.a )
execute_process( COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC_DIR}/FreeImage.h ${DEST_DIR}/include/FreeImage.h )
