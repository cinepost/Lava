#set( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fPIC" )
#set( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fPIC" )

#set( ANTLR4_SOURCE_DIR
#    ${PROJECT_SOURCE_DIR}/third_party/antlr4/runtime/Cpp/runtime/src
#)

#find_package(ANTLR REQUIRED)
#if( ANTLR_FOUND )
#    include_directories(${ANTLR4_INCLUDE_DIRS})
#    #antlr_target( lava_lib ${CMAKE_CURRENT_SOURCE_DIR}/LSD.g4 )
#else()
#    message( "ANTLR NOT FOUND !!!" )
#endif()

#set( GENERATED_SOURCES
#    ${CMAKE_CURRENT_SOURCE_DIR}/readers/generated/LSDBaseListener.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/readers/generated/LSDBaseVisitor.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/readers/generated/LSDListener.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/readers/generated/LSDVisitor.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/readers/generated/LSDParser.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/readers/generated/LSDLexer.cpp
#)

#foreach( src_file ${GENERATED_SOURCES} )
#    set_source_files_properties(
#        ${src_file} PROPERTIES GENERATED TRUE
#    )
#endforeach( src_file ${GENERATED_SOURCES} )

#add_custom_target( GenerateParsers DEPENDS ${GENERATED_SOURCES} )
#add_custom_command( OUTPUT ${GENERATED_SOURCES}
#    COMMAND 
#    ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/readers/generated/
#    
#    COMMAND
#    java -jar ${ANTLR_EXECUTABLE} -Werror -Dlanguage=Cpp -visitor -o ${CMAKE_CURRENT_SOURCE_DIR}/readers/generated/ -package lava ${CMAKE_CURRENT_SOURCE_DIR}/LSD.g4
#)

#include_directories(
#    ${ANTLR4_SOURCE_DIR}
#    ${ANTLR4_SOURCE_DIR}/misc
#    ${ANTLR4_SOURCE_DIR}/atn
#    ${ANTLR4_SOURCE_DIR}/dfa
#    ${ANTLR4_SOURCE_DIR}/tree
#    ${ANTLR4_SOURCE_DIR}/support
#    ${CMAKE_CURRENT_SOURCE_DIR}/readers/generated
#)

file( GLOB SOURCES
    ./*.cpp
    ./readers/*.cpp
#    ${GENERATED_SOURCES}
)

#if (NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
#  set (flags_1 "-Wno-overloaded-virtual")
#else()
#  set (flags_1 "-MP /wd4251")
#endif()

#foreach( src_file ${GENERATED_SOURCES} )
#    set_source_files_properties(
#        ${src_file} PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} ${flags_1}"
#    )
#endforeach( src_file ${GENERATED_SOURCES} )

# Python
find_package( PythonLibs 3.7 REQUIRED )
include_directories( ${PYTHON_INCLUDE_DIRS} )

# FreeImage
find_package(FreeImage REQUIRED)
include_directories( ${FREEIMAGE_INCLUDE_DIRS} )

# Find Boost libs
find_package( Boost COMPONENTS system filesystem REQUIRED )
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS}) 
endif()

include_directories( ${Boost_INCLUDE_DIRS} )

# We use Boost static libs
add_definitions( -DBOOST_LOG_DYN_LINK )

add_library( lava_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    lava_lib
    falcor_lib
    lava_utils_lib
    ${Boost_LIBRARIES}
#    antlr4_shared
)

add_dependencies( lava_lib GenerateParsers )

#add_custom_command(
#    TARGET lava_lib
#    POST_BUILD
#    COMMAND ${CMAKE_COMMAND}
#       -E copy ${ANTLR4_RUNTIME_LIBRARIES} .
#    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
#)


if(APPLE)
    if( LAVA_BUILD_LIBS_AS_FRAMEWORKS )
	   #falcor_config_framework( lava_lib )

        # Framework is called 'Lava'
        set_target_properties( lava_lib PROPERTIES OUTPUT_NAME "Lava" )
        install( TARGETS lava_lib DESTINATION "Frameworks" )
    else()
        install( TARGETS lava_lib DESTINATION "lib" )
    endif()

elseif(UNIX)
    install(TARGETS lava_lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    )
    #install(TARGETS antlr4_shared
    #    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    #    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    #    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    #)
endif()
