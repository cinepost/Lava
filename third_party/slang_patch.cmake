# patch slang
execute_process(COMMAND bash  "-c" "sed -i 's/SpvOpConstFunctionPointerINTEL/SpvOpConstantFunctionPointerINTEL/' ${SOURCE_DIR}/external/spirv-tools-generated/core.insts-unified1.inc "
    ERROR_VARIABLE ERROR_MESSAGE
    RESULT_VARIABLE ERROR_CODE
    OUTPUT_FILE "/proc/self/fd/0"
)