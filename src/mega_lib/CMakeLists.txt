set(DEPS_DIR "${EXTERNALS_INSTALL_DIR}/")

include(${PROJECT_SOURCE_DIR}/third_party/external_versions.cmake)


add_library( mega_lib SHARED null.cpp )

# Find Blosc
set(Blosc_ROOT ${DEPS_DIR})
find_package(Blosc ${EXTERNAL_BLOSC_VERSION} EXACT REQUIRED)
message("Bundling mega lib with Boost libraries: " ${Boost_LIBRARIES})

# Find Boost
set(Boost_USE_STATIC_LIBS ON)
set(BOOST_ROOT ${DEPS_DIR})
find_package( Boost ${EXTERNAL_BOOST_VERSION} COMPONENTS log log_setup thread filesystem system program_options EXACT REQUIRED)
target_include_directories(mega_lib PUBLIC ${Boost_INCLUDE_DIRS})
message("Bundling mega lib with Boost libraries: " ${Boost_LIBRARIES})

# OpenEXR
set(OPENEXR_USE_STATIC_LIBS ON)
set(OpenEXR_ROOT ${DEPS_DIR})
find_package(OpenEXR ${EXTERNAL_OPENEXR_VERSION} EXACT REQUIRED)
target_include_directories(mega_lib PUBLIC ${OpenEXR_INCLUDE_DIRS})
message("Bundling mega lib with OpenEXR libraries: " OpenEXR::IlmImf)

# Assimp
set(assimp_ROOT ${DEPS_DIR})
find_package(assimp ${EXTERNAL_ASSIMP_VERSION} EXACT REQUIRED)
target_include_directories(mega_lib PUBLIC ${ASSIMP_INCLUDE_DIR})
message("Bundling mega lib with Assimp libraries: " ${assimp_LIBRARIES})

# OpenVDB
set(OPENVDB_USE_STATIC_LIBS ON)
set(OpenVDB_ROOT ${DEPS_DIR})
find_package(OpenVDB ${EXTERNAL_OPENVDB_VERSION} EXACT REQUIRED)
message("Bundling mega lib with OpenVDB libraries: " OpenVDB::openvdb)

# Find OpenImageDenoiser
set(OIDN_ROOT ${DEPS_DIR})
find_package(OIDN ${EXTERNAL_OIDN_VERSION} EXACT REQUIRED)
message("Bundling mega lib with OpenImageDenoise libraries: " ${OIDN_LIBRARY} " " ${DNNL_LIBRARY})

# Find OpenImageIO
set(OpenImageIO_ROOT ${DEPS_DIR})
find_package(OpenImageIO ${EXTERNAL_OIIO_VERSION} EXACT REQUIRED)
target_include_directories(mega_lib PUBLIC ${OPENIMAGEIO_INCLUDES} )
message("Bundling mega lib with OpenImageIO libraries: " OpenImageIO::OpenImageIO)

# Find Freeimage
set(FreeImage_ROOT ${DEPS_DIR})
find_package(FreeImage ${FREEIMAGE_VERSION} EXACT REQUIRED)
target_include_directories(mega_lib PUBLIC ${FreeImage_INCLUDE_DIRS} )
message("Bundling mega lib with FreeImage libraries: " FreeImage::FreeImage)

# Find GLFW3
set(GLFW_ROOT ${DEPS_DIR})
find_package(GLFW ${EXTERNAL_GLFW_VERSION} EXACT REQUIRED)
target_include_directories(mega_lib PUBLIC ${GLFW_INCLUDE_DIR} )
message("Bundling mega lib with GLFW library: " ${GLFW_LIBRARY})

#set(LINK_FLAGS ${LINK_FLAGS} "-static-libgcc -static-libstdc++")
#set(CMAKE_SHARED_LINKER_FLAGS "-static-libgcc -static-libstdc++")

target_link_libraries( mega_lib
    PRIVATE
    "-Wl,--as-needed" Boost::filesystem
    "-Wl,--as-needed" Boost::log
    "-Wl,--as-needed" Boost::system
    "-Wl,--as-needed" Boost::thread
    "-Wl,--as-needed" Boost::program_options
#    Blosc::blosc
#    "-Wl,-no-whole-archive"
#    ${Boost_LIBRARIES}
#    "-Wl,-whole-archive"
#    OpenEXR::IlmImf
#    "-Wl,-no-whole-archive"
#    FreeImage::FreeImage
#    "-Wl,-no-whole-archive"
#    OpenVDB::openvdb
#    "-Wl,-whole-archive"
#    ${assimp_LIBRARIES}
#    "-Wl,-no-whole-archive"
#    ${DNNL_LIBRARY}
#    "-Wl,-whole-archive"
#    ${OIDN_LIBRARY}
#    "-Wl,-no-whole-archive"
    OpenImageIO::OpenImageIO
#    "-Wl,-no-whole-archive"
#    ${GLFW_LIBRARY}
#    "-Wl,-no-whole-archive"
)

set_target_properties(mega_lib PROPERTIES
    C_VISIBILITY_PRESET       default
    CXX_VISIBILITY_PRESET     default
    VISIBILITY_INLINES_HIDDEN YES
)

if(UNIX)
    install( TARGETS mega_lib DESTINATION "lib" )
endif()