execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${VMA_INST_DIR})
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VMA_SRC_DIR}/src/vk_mem_alloc.h ${VMA_INST_DIR}/vk_mem_alloc.h)
