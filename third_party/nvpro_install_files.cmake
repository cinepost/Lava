execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${NVPRO_INST_DIR}/nvvk)
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${NVPRO_INST_DIR}/nvp)
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${NVPRO_INST_DIR}/nvh)
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${NVPRO_INST_DIR}/nvmath)

execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${NVPRO_SRC_DIR}/nvvk ${NVPRO_INST_DIR}/nvvk)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${NVPRO_SRC_DIR}/nvp ${NVPRO_INST_DIR}/nvp)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${NVPRO_SRC_DIR}/nvh ${NVPRO_INST_DIR}/nvh)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${NVPRO_SRC_DIR}/nvmath ${NVPRO_INST_DIR}/nvmath)
