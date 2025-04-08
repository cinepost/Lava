set(DEPS_DIR "${EXTERNALS_INSTALL_DIR}/")

include_directories( 
    ${EXTERNALS_INSTALL_DIR}/include
    ${EXTERNALS_INSTALL_DIR}/include/slang
    ${PROJECT_SOURCE_DIR}/external/include
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source
)

file( GLOB SOURCES
    ./*.cpp
    ./vulkan/*.cpp
    ./debug-layer/*.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-crypto.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-string.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-string-util.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-string-escape-util.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-blob.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-uint-set.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-char-util.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-char-encode.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-signal.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-persistent-cache.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-stream.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-file-system.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-io.cpp
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-text-io.cpp
)

if(WIN32)
    file( GLOB PLATFORM_SOURCES
        ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/windows/slang-win-process.cpp
    )
else()
    file( GLOB PLATFORM_SOURCES
        ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/unix/slang-unix-process.cpp
    )
endif()

# SLANG
find_library( SLANG_LIBRARY slang PATHS ${DEPS_DIR}/lib )

add_library( gfx_lib SHARED ${SOURCES} ${PLATFORM_SOURCES} ${HEADERS} )

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
        PUBLIC
            lava_utils_lib
        PRIVATE
            "stdc++fs"
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
