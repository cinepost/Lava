set( ANTLR4_SOURCE_DIR
    ${PROJECT_SOURCE_DIR}/third_party/antlr4/runtime/Cpp/runtime/src
)

set( GENERATED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/generated/LSDBaseVisitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/generated/LSDVisitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/generated/LSDParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/generated/LSDLexer.cpp
)

foreach( src_file ${GENERATED_SOURCES} )
    set_source_files_properties(
        ${src_file} PROPERTIES GENERATED TRUE
    )
endforeach( src_file ${GENERATED_SOURCES} )

add_custom_command( OUTPUT ${GENERATED_SOURCES}
    COMMAND 
    ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/loaders/generated/
    
    COMMAND
    java -jar ${ANTLR4CPP_JAR_LOCATION} -Werror -Dlanguage=Cpp -visitor -o ${CMAKE_CURRENT_SOURCE_DIR}/loaders/generated/ -package lava ${CMAKE_CURRENT_SOURCE_DIR}/LSD.g4
)

add_custom_target( GenerateParsers DEPENDS ${GENERATED_SOURCES} )

include_directories(
    ${ANTLR4_SOURCE_DIR}
    ${ANTLR4_SOURCE_DIR}/misc
    ${ANTLR4_SOURCE_DIR}/atn
    ${ANTLR4_SOURCE_DIR}/dfa
    ${ANTLR4_SOURCE_DIR}/tree
    ${ANTLR4_SOURCE_DIR}/support
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/generated
)

file( GLOB SOURCES
    ./*.cpp
    ./loaders/*.cpp
    ${GENERATED_SOURCES}
)

if (NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set (flags_1 "-Wno-overloaded-virtual")
else()
  set (flags_1 "-MP /wd4251")
endif()

foreach( src_file ${GENERATED_SOURCES} )
    set_source_files_properties(
        ${src_file} PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} ${flags_1}"
    )
endforeach( src_file ${GENERATED_SOURCES} )

# Python
find_package( PythonLibs 3.7 REQUIRED )
include_directories( ${PYTHON_INCLUDE_DIRS} )

# FreeImage
find_package(FreeImage REQUIRED)
include_directories( ${FREEIMAGE_INCLUDE_DIRS} )

# Find Boost libs
find_package( Boost COMPONENTS system filesystem REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )

# We use Boost static libs
add_definitions( -DBOOST_LOG_DYN_LINK )


add_library( lava_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    lava_lib
    falcor_lib
    lava_utils_lib
    antlr4_static
)

add_dependencies( lava_lib GenerateParsers )


if(APPLE)
    if( COPPER_BUILD_LIBS_AS_FRAMEWORKS )
	#falcor_config_framework( lava_lib )

    # Framework is called 'Falcor'
    set_target_properties( lava_lib PROPERTIES OUTPUT_NAME "Falcor" )
    install( TARGETS lava_lib DESTINATION "Frameworks" )
  else()
    install( TARGETS lava_lib DESTINATION "lib" )
  endif()

elseif(UNIX)
    install( TARGETS lava_lib DESTINATION lib )
endif()
