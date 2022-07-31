set(DEPS_DIR "${EXTERNALS_INSTALL_DIR}/")

include_directories( 
    ${EXTERNALS_INSTALL_DIR}/include
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source
)

file( GLOB SOURCES
    ./*.cpp
    ./vulkan/*.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-string.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-blob.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-uint-set.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-char-util.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-char-encode.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-signal.cpp
)

# Find Boost libs
set(Boost_NO_SYSTEM_PATHS ON)
set(BOOST_ROOT ${DEPS_DIR})
find_package( Boost COMPONENTS log REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )

# SLANG
find_library( SLANG_LIBRARY slang PATHS ${DEPS_DIR}/lib )

add_library( gfx_lib SHARED ${SOURCES} ${HEADERS} )

target_link_libraries(
    gfx_lib
    lava_utils_lib
    ${SLANG_LIBRARY}
)

if(UNIX)
    install(TARGETS gfx_lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    )
endif()
