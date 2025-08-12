if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(ASSIMP_ARCHITECTURE "64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(ASSIMP_ARCHITECTURE "32")
endif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	
if(WIN32)
	set(assimp_ROOT CACHE PATH "ASSIMP root directory")

	# Find path of each library
	find_path(ASSIMP_INCLUDE_DIR
		NAMES
			assimp/anim.h
		HINTS
			${assimp_ROOT}/include
	)

	set(ASSIMP_MSVC_VERSION vc${MSVC_TOOLSET_VERSION})
	
	message("ASSIMP_MSVC_VERSION " ${ASSIMP_MSVC_VERSION})
	
	find_path(ASSIMP_LIBRARY_DIR
		NAMES
			assimp-${ASSIMP_MSVC_VERSION}-mt.lib
			assimp-${ASSIMP_MSVC_VERSION}-mtd.lib
		HINTS
			${assimp_ROOT}/lib
	)
	
	find_library(ASSIMP_LIBRARY_RELEASE				assimp-${ASSIMP_MSVC_VERSION}-mt.lib 			PATHS ${ASSIMP_LIBRARY_DIR})
	find_library(ASSIMP_LIBRARY_DEBUG				assimp-${ASSIMP_MSVC_VERSION}-mtd.lib			PATHS ${ASSIMP_LIBRARY_DIR})
	
	set(ASSIMP_LIBRARY 
		optimized 	${ASSIMP_LIBRARY_RELEASE}
		debug		${ASSIMP_LIBRARY_DEBUG}
	)

	message("ASSIMP_LIBRARY " ${ASSIMP_LIBRARY})
	
	set(ASSIMP_LIBRARIES "ASSIMP_LIBRARY_RELEASE" "ASSIMP_LIBRARY_DEBUG")

	if(NOT ASSIMP_INCLUDE_DIR)
		set(assimp_FOUND FALSE)
   		return()
	endif()

	if(NOT ASSIMP_LIBRARIES)
		set(assimp_FOUND FALSE)
   		return()
   	endif()

   	set(assimp_FOUND TRUE)

	FUNCTION(ASSIMP_COPY_BINARIES TargetDirectory)
		ADD_CUSTOM_TARGET(AssimpCopyBinaries
			COMMAND ${CMAKE_COMMAND} -E copy ${assimp_ROOT}/bin/assimp-${ASSIMP_MSVC_VERSION}-mtd.dll 	${TargetDirectory}/Debug/assimp-${ASSIMP_MSVC_VERSION}-mtd.dll
			COMMAND ${CMAKE_COMMAND} -E copy ${assimp_ROOT}/bin/assimp-${ASSIMP_MSVC_VERSION}-mt.dll 	${TargetDirectory}/Release/assimp-${ASSIMP_MSVC_VERSION}-mt.dll
		COMMENT "Copying Assimp binaries to '${TargetDirectory}'"
		VERBATIM)
	ENDFUNCTION(ASSIMP_COPY_BINARIES)

	if (NOT TARGET ASSIMP)
		set(INCLUDE_DIRS ${assimp_ROOT}/include)

		find_library(ASSIMP_LIB_DEBUG
			NAMES assimp-${ASSIMP_MSVC_VERSION}-mtd.lib
			PATHS ${ASSIMP_LIBRARY_DIR})

		find_file(ASSIMP_DLL_DEBUG
			NAMES assimp-${ASSIMP_MSVC_VERSION}-mtd.dll
			PATHS ${assimp_ROOT}/bin)

		find_library(ASSIMP_LIB_RELEASE
			NAMES assimp-${ASSIMP_MSVC_VERSION}-mt.lib
			PATHS ${ASSIMP_LIBRARY_DIR})

		find_file(ASSIMP_DLL_RELEASE
			NAMES assimp-${ASSIMP_MSVC_VERSION}-mt.dll
			PATHS ${assimp_ROOT}/bin)

		add_library(ASSIMP SHARED IMPORTED)
		set_target_properties(ASSIMP PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${INCLUDE_DIRS}"
			IMPORTED_IMPLIB_DEBUG ${ASSIMP_LIB_DEBUG}
			IMPORTED_IMPLIB_RELEASE ${ASSIMP_LIB_RELEASE}
			IMPORTED_LOCATION_DEBUG ${ASSIMP_DLL_DEBUG}
			IMPORTED_LOCATION_RELEASE ${ASSIMP_DLL_RELEASE}
		)
	endif()
	
else(WIN32)

	find_path(
	  assimp_INCLUDE_DIRS
	  NAMES assimp/postprocess.h assimp/scene.h assimp/version.h assimp/config.h assimp/cimport.h
	  PATHS ${assimp_ROOT}/include/
	)

	find_library(
	  assimp_LIBRARIES
	  NAMES libassimp.a assimp assimpd
	  PATHS ${assimp_ROOT}
	)

	if (assimp_INCLUDE_DIRS AND assimp_LIBRARIES)
	  SET(assimp_FOUND TRUE)
	ENDIF (assimp_INCLUDE_DIRS AND assimp_LIBRARIES)

	if (assimp_FOUND)
	  if (NOT assimp_FIND_QUIETLY)
		message(STATUS "Found asset importer library: ${assimp_LIBRARIES}")
	  endif (NOT assimp_FIND_QUIETLY)
	else (assimp_FOUND)
	  if (assimp_FIND_REQUIRED)
		message(FATAL_ERROR "Could not find asset importer library")
	  endif (assimp_FIND_REQUIRED)
	endif (assimp_FOUND)
	
endif(WIN32)