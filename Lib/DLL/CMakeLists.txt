cmake_minimum_required(VERSION 2.6)
project(RakNetDLL)

FILE(GLOB ALL_HEADER_SRCS ${RakNet_SOURCE_DIR}/Source/*.h)
FILE(GLOB ALL_CPP_SRCS   ${RakNet_SOURCE_DIR}/Source/*.cpp)

include_directories( ${RAKNET_INTERNAL_INCLUDE_DIRS} )

IF(WIN32 AND NOT UNIX)
	# MSVC build: include the Windows resource file for DLL version info
	IF(MSVC)
		add_library(RakNetDLL SHARED ${ALL_CPP_SRCS} ${ALL_HEADER_SRCS} DLL.rc)
		SET(CMAKE_CXX_FLAGS "/D WIN32 /D _RAKNET_DLL /D _CRT_NONSTDC_NO_DEPRECATE /D _CRT_SECURE_NO_DEPRECATE /GS- /GR-")
	ELSE()
		# MinGW cross-compile: no .rc file, use GCC-style flags
		add_library(RakNetDLL SHARED ${ALL_CPP_SRCS} ${ALL_HEADER_SRCS})
		target_compile_options(RakNetDLL PRIVATE
			-D_RAKNET_DLL
			-DWIN32
			-fvisibility=default
		)
		target_include_directories(RakNetDLL PRIVATE ${RakNet_SOURCE_DIR}/mingw-case-fix)
		set_target_properties(RakNetDLL PROPERTIES PREFIX "")
	ENDIF()

	# Export macro for our C wrapper
	target_compile_definitions(RakNetDLL PRIVATE RAKNET_C_EXPORTS)
	target_link_libraries(RakNetDLL ${RAKNET_LIBRARY_LIBS})

ELSE(WIN32 AND NOT UNIX)
	# Native Linux/macOS build
	add_library(RakNetDLL SHARED ${ALL_CPP_SRCS} ${ALL_HEADER_SRCS})

	# Export macro + make all extern "C" symbols visible
	target_compile_definitions(RakNetDLL PRIVATE RAKNET_C_EXPORTS)
	target_compile_options(RakNetDLL PRIVATE -fvisibility=default)
	target_link_libraries(RakNetDLL ${RAKNET_LIBRARY_LIBS})

	INSTALL(TARGETS RakNetDLL DESTINATION ${RakNet_SOURCE_DIR}/Lib/DLL)
ENDIF(WIN32 AND NOT UNIX)
