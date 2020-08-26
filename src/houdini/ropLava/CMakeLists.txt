# Locate Houdini's libraries and header files.
# Registers an imported library target named 'Houdini'.
list( APPEND CMAKE_PREFIX_PATH "$ENV{HFS}/toolkit/cmake" )
find_package( Houdini REQUIRED )

# Add a library with two source files.
set( library_name ROP_Lava )

add_library( ${library_name} SHARED
    ROP_Lava.cpp
    ROP_Lava.h
)

# Link against the Houdini libraries, and add required include directories and compile definitions.
target_link_libraries( 
	${library_name} 
	lava_utils_lib
	Houdini

)

# Configure several common target properties, such as its output directory.
houdini_configure_target( ${library_name} )