cmake_minimum_required(VERSION 3.18)

# ---- Project ----

project(
	Buffout4
	VERSION 1.4.0
	LANGUAGES CXX
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY
)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(
		FATAL_ERROR
			"In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
)
endif()

# ---- Globals ----

set(MSVC_OPTIONS_GLOBAL
	/MP	# Build with Multiple Processes
	/Zi	# Debug Information Format
	
	/permissive-	# Standards conformance
	
	/Zc:alignedNew	# C++17 over-aligned allocation
	/Zc:auto	# Deduce Variable Type
	/Zc:char8_t
	/Zc:__cplusplus	# Enable updated __cplusplus macro
	/Zc:externC
	/Zc:externConstexpr	# Enable extern constexpr variables
	/Zc:forScope	# Force Conformance in for Loop Scope
	/Zc:hiddenFriend
	/Zc:implicitNoexcept	# Implicit Exception Specifiers
	/Zc:lambda
	/Zc:noexceptTypes	# C++17 noexcept rules
	/Zc:preprocessor	# Enable preprocessor conformance mode
	/Zc:referenceBinding	# Enforce reference binding rules
	/Zc:rvalueCast	# Enforce type conversion rules
	/Zc:sizedDealloc	# Enable Global Sized Deallocation Functions
	/Zc:strictStrings	# Disable string literal type conversion
	/Zc:ternary	# Enforce conditional operator rules
	/Zc:threadSafeInit	# Thread-safe Local Static Initialization
	/Zc:tlsGuards
	/Zc:trigraphs	# Trigraphs Substitution
	/Zc:wchar_t	# wchar_t Is Native Type
	
	/experimental:external
	/external:anglebrackets
	/external:W0
	
	/W4	# Warning level (all warnings)
	/WX	# Warning level (warnings are errors)
)

add_compile_definitions(
	"$<$<BOOL:${MSVC}>:_UNICODE>"
	TOML_WINDOWS_COMPAT=0
)

add_compile_options(
	"$<$<BOOL:${MSVC}>:${MSVC_OPTIONS_GLOBAL}>"
)

# ---- Dependencies ----

set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_USE_DEBUG_RUNTIME ON)

add_subdirectory($ENV{CommonLibF4Path} CommonLibF4)

find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(frozen REQUIRED)
find_package(spdlog REQUIRED)
find_package(tomlplusplus REQUIRED)

find_path(XBYAK_INCLUDE_DIRS "xbyak/xbyak.h")

# ---- Add source files ----

set(headers
	include/AutoTOML.hpp	# TODO: convert to a cmake project
	src/Crash/Modules/ModuleHandler.h
	src/Crash/Stack/StackHandler.h
	src/Crash/CrashHandler.h
	src/Fixes/EncounterZoneResetFix.h
	src/Fixes/FaderMenuFix.h
	src/Fixes/Fixes.h
	src/Fixes/SafeExit.h
	src/Fixes/ScaleformAllocatorFix.h
	src/Fixes/SmallBlockAllocatorFix.h
	src/PCH.h
	src/Settings.h
)

set(sources
	src/Crash/Modules/ModuleHandler.cpp
	src/Crash/Stack/StackHandler.cpp
	src/Crash/CrashHandler.cpp
	src/Fixes/Fixes.cpp
	src/Fixes/SmallBlockAllocatorFix.cpp
	src/main.cpp
	src/PCH.cpp
)

source_group(
	TREE ${CMAKE_CURRENT_SOURCE_DIR}
	FILES ${headers} ${sources}
)

# ---- Create DLL ----

set(MSVC_OPTIONS_PRIVATE
	/TP	# Specify Source File Type (C++)
)

add_library(Buffout4 SHARED
	${headers}
	${sources}
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	.clang-format
	Buffout4.toml
)

set_property(
	GLOBAL
	PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

target_compile_features(Buffout4
	PUBLIC
		cxx_std_17
)

target_compile_options(Buffout4
	PRIVATE
		"$<$<BOOL:${MSVC}>:${MSVC_OPTIONS_PRIVATE}>"
)

target_include_directories(Buffout4
	PRIVATE 
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:src>
		${XBYAK_INCLUDE_DIRS}
)

target_link_libraries(Buffout4
	PUBLIC
		Boost::headers
		CommonLibF4
		fmt::fmt
		frozen::frozen
		Dbghelp.lib
		spdlog::spdlog
		tomlplusplus::tomlplusplus
)

target_link_options(Buffout4
	PUBLIC
		"$<$<AND:$<BOOL:${MSVC}>,$<CONFIG:RELEASE>>:/DEBUG:FULL>"
)

target_precompile_headers(Buffout4
	PRIVATE
		src/PCH.h
)

# ---- Post build ----

add_custom_command(
	TARGET Buffout4
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Buffout4> $ENV{Fallout4Path}/Data/F4SE/Plugins/
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:Buffout4> $ENV{Fallout4Path}/Data/F4SE/Plugins/
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/Buffout4.toml $ENV{Fallout4Path}/Data/F4SE/Plugins/
)
