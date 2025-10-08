if(NOT EXTERNALS_INSTALL_DIR)
    set(EXTERNALS_INSTALL_DIR ${PROJECT_SOURCE_DIR}/deps/${CMAKE_BUILD_TYPE})
endif()

set(DEPS_DIR "${EXTERNALS_INSTALL_DIR}/")

include_directories( 
    ${EXTERNALS_INSTALL_DIR}/include
    ${EXTERNALS_INSTALL_DIR}/include/slang
    ${PROJECT_SOURCE_DIR}/external/include
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source
    ${Boost_INCLUDE_DIRS}
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
    ${CMAKE_THIRD_PARTY_SOURCE_DIR}/external/slang/src/Slang/source/core/slang-command-line.cpp
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

set(DISABLE_CMAKE_SEARCH_PATHS TRUE)

# SLANG
find_library( SLANG_LIBRARY slang PATHS ${DEPS_DIR}/lib )

add_library( gfx_lib SHARED ${SOURCES} ${PLATFORM_SOURCES} ${HEADERS} )

if(WIN32)
    find_package(Vulkan REQUIRED)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
    add_definitions(-DBOOST_ALL_DYN_LINK)
    add_definitions(-DBOOST_LOG_DYN_LINK)

    set_target_properties(gfx_lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    # target_link_options(gfx_lib PRIVATE "/FORCE:UNRESOLVED")

    target_link_libraries(
        gfx_lib
        PUBLIC
            lava_utils_lib
            Vulkan::Vulkan
        PRIVATE
            ${SLANG_LIBRARY}
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

if(UNIX)
    install(
        TARGETS gfx_lib
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
elseif(WIN32)
    install(
        TARGETS gfx_lib
        RUNTIME DESTINATION bin
    )
endif()
