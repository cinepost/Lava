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

# SLANG
find_library( SLANG_LIBRARY slang PATHS ${DEPS_DIR}/lib )

add_library( gfx_lib SHARED ${SOURCES} ${HEADERS} )

if(WIN32)
    find_package(Vulkan REQUIRED)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
    add_definitions(-DBOOST_ALL_DYN_LINK)
    find_library( SLANG_GFX_LIBRARY gfx PATHS ${DEPS_DIR}/lib )

    set_target_properties(gfx_lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    # target_link_options(gfx_lib PRIVATE "/FORCE:UNRESOLVED")

target_link_libraries(
    gfx_lib
    PUBLIC
    lava_utils_lib
    ${SLANG_LIBRARY}
    ${SLANG_GFX_LIBRARY}
    Vulkan::Vulkan
    PRIVATE
    # falcor_lib "../../Release/lib/Release/falcor_lib"
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/falcor_lib.lib
)
else()
target_link_libraries(
    gfx_lib
    lava_utils_lib
    ${SLANG_LIBRARY}
)
endif()

if(UNIX OR WIN32)
    install(TARGETS gfx_lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    )
endif()
