if(PKG_CONFIG_EXECUTABLE)
    set( PkgConfigWin32_FOUND TRUE )
    return()
endif()

if( NOT PkgConfigWin32_BINDIR )
    message( FATAL_ERROR "PkgConfigWin32_BINDIR not set !!!")
else()
    message( "Finding pkg_config in " ${PkgConfigWin32_BINDIR})
endif()

find_library ( 
    INTL_DLL_LIB
    NAMES 
        intl
        intl.dll 
    PATHS 
        ${PkgConfigWin32_BINDIR}
    NO_DEFAULT_PATH 
)

if( NOT INTL_DLL_LIB )
    set( PkgConfigWin32_FOUND FALSE )
    return()
endif()

find_program( 
    PKG_CONFIG_EXECUTABLE
    NAMES 
        pkg-config
        pkg-config.exe 
    PATHS 
        ${PkgConfigWin32_BINDIR}
    NO_DEFAULT_PATH 
)

if( NOT PKG_CONFIG_EXECUTABLE )
    set( PkgConfigWin32_FOUND FALSE )
    return()
endif()

set( PkgConfigWin32_FOUND TRUE )