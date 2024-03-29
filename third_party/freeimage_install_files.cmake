if(STATIC_LIB)
	execute_process( COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC_DIR}/libfreeimage.a ${DEST_DIR}/lib/libfreeimage.a )
else()
	if(WIN32)
	execute_process( COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC_DIR}/x64/Release/FreeImage.lib ${DEST_DIR}/lib/FreeImage.lib )
	execute_process( COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC_DIR}/x64/Release/FreeImage.dll ${DEST_DIR}/bin/FreeImage.dll )
	else()
	execute_process( COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC_DIR}/libfreeimage-3.18.0.so ${DEST_DIR}/lib/libfreeimage-3.18.0.so )
	SET(TARGET_LIB ${DEST_DIR}/lib/libfreeimage-3.18.0.so)
	SET(LINK_LIB ${DEST_DIR}/lib/libfreeimage.so.3)
	execute_process( COMMAND ${CMAKE_COMMAND} -E create_symlink ${TARGET_LIB} ${LINK_LIB} )
	endif()
endif()
if(WIN32)
execute_process( COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC_DIR}/Source/FreeImage.h ${DEST_DIR}/include/FreeImage.h )
else()
execute_process( COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRC_DIR}/FreeImage.h ${DEST_DIR}/include/FreeImage.h )
endif()
