# copy libs
file(GLOB TBB_LIBRARY_FILES
  ${TBB_LIBS_DIR}/libtbb*
)

file(COPY ${TBB_LIBRARY_FILES} DESTINATION ${DEST_DIR}/lib)

# copy headers
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${TBB_INCL_DIR} ${DEST_DIR}/include/tbb)
if(WIN32)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${TBB_INCL_DIR}/../oneapi ${DEST_DIR}/include/oneapi)
endif()
