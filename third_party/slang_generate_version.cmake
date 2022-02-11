# Git executable is extracted from parameters.
execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --always 
    OUTPUT_VARIABLE SLANG_GIT_REPO_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(STRIP ${SLANG_GIT_REPO_VERSION} SLANG_GIT_REPO_VERSION)

# Input and output files are extracted from parameters.
configure_file(${INPUT_FILE} ${OUTPUT_FILE})