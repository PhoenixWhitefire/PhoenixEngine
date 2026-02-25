# CMakeLists.txt for Phoenix Engine
# https://github.com/PhoenixWhitefire/PhoenixEngine

cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_VS_PLATFORM_TOOLSET v143)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# visual studio solution filter folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Debug: Standard debug mode. Incremental linking. Tracy and ASan enabled
# Release: Optimizations, LTCG, no Incremental linking, ASan disabled, Tracy in ON_DEMAND mode
# but for distribution binaries to be `Release` for best performance (because Tracy introduces it's own overhead)
# 19/01/2025
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;DebugTSan;ReleaseTSan")

project(PhoenixEngine LANGUAGES C CXX)

file(GLOB_RECURSE PHX_SRCS
	"src/decl/*.hpp"
	"src/impl/*.cpp"
)

add_executable(PhoenixEngine ${PHX_SRCS})

source_group(TREE src FILES ${PHX_SRCS})
target_sources(PhoenixEngine PRIVATE README.md phoenix.conf)

set_target_properties(PhoenixEngine PROPERTIES OUTPUT_NAME PhoenixEngine)

target_compile_definitions(PhoenixEngine PRIVATE $<$<CONFIG:Release>:NDEBUG TRACY_ON_DEMAND>)
target_compile_definitions(PhoenixEngine PRIVATE PHX_TARGET_PLATFORM="${CMAKE_SYSTEM_NAME}" TRACY_ENABLE TRACY_FIBERS)

option(PHX_HEADLESS "Build in Headless mode" OFF)

if (PHX_HEADLESS)
	target_compile_definitions(PhoenixEngine PRIVATE PHX_HEADLESS_BUILD=1)
else()
	target_compile_definitions(PhoenixEngine PRIVATE PHX_HEADLESS_BUILD=0)
endif()

target_include_directories(PhoenixEngine PUBLIC
	"./"
	"src/decl"
	"Vendor"
	"Vendor/glm"
	"Vendor/imgui"
	"Vendor/tracy/public"
	"Vendor/luau/Common/include"
	"Vendor/luau/VM/include"
	"Vendor/luau/Compiler/include"
	"Vendor/luau/Ast/include"
	"Vendor/glfw/include"
	"Vendor/tinyfd"
	"Vendor/curl/include"
)

set(PHX_BUILD_TYPE $<IF:$<CONFIG:Debug>,Debug,$<IF:$<CONFIG:Release>,Release,$<IF:$<CONFIG:DebugTSan>,DebugTSan,ReleaseTSan>>>)

target_compile_definitions(PhoenixEngine PRIVATE PHX_BUILD_TYPE="${PHX_BUILD_TYPE}")

if (MSVC)
	include(cmake/msvc.txt)
	target_compile_definitions(PhoenixEngine PRIVATE PHX_TARGET_COMPILER="MSVC")
	target_compile_options(PhoenixEngine PRIVATE /wd4244)

	message(STATUS "Recognized compiler as MSVC")

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	include(cmake/gnug.txt)
	target_compile_definitions(PhoenixEngine PRIVATE PHX_TARGET_COMPILER="G++")
	target_link_libraries(PhoenixEngine PRIVATE X11) # audible sigh when i checked `imgui_impl_glfw.cpp` line 1266

	message(STATUS "Recognized compiler as G++")

else()
	message(WARNING "Unsupported compiler - expected either MSVC or GNU (G++)")
endif()

target_link_libraries(PhoenixEngine PRIVATE
	Vendor
	OpenGL
	glm
	miniaudio
	$<IF:$<BOOL:${PHX_HEADLESS}>,,glfw>
	TracyClient
	libcurl

	Luau.Compiler
	Luau.Ast
	Luau.VM
	Luau.Require
	Luau.Config
)

# not letting me put multiple definitions in the same expression waaa
add_compile_definitions(TRACY_ENABLE TRACY_FIBERS)
add_compile_definitions($<$<CONFIG:Release>:NDEBUG>)
add_compile_definitions($<$<CONFIG:Release>:TRACY_ON_DEMAND>)

# remove unnecessary projects
option(LUAU_BUILD_CLI "Build CLI" OFF)
option(LUAU_BUILD_TESTS "Build tests" OFF)
option(MINIAUDIO_NO_EXTRA_NODES                "Do not build extra node graph nodes" ON)
option(BUILD_CURL_EXE "Build curl executable" OFF)
option(BUILD_STATIC_LIBS "Build static libraries" ON)
option(HTTP_ONLY "Disable all protocols except HTTP (This overrides all CURL_DISABLE_* options)" ON)
option(BUILD_LIBCURL_DOCS "Build libcurl man pages" OFF)
option(BUILD_MISC_DOCS "Build misc man pages (e.g. curl-config and mk-ca-bundle)" OFF)
option(ENABLE_CURL_MANUAL "Build the man page for curl and enable its -M/--manual option" OFF)
option(BUILD_TESTING "Build tests" OFF)
option(BUILD_EXAMPLES "Build libcurl examples" OFF)
option(CURL_USE_LIBPSL "Use libpsl" OFF) # TODO figure out how to install this on Windows github action runners

add_subdirectory(Vendor)
add_subdirectory(Vendor/luau EXCLUDE_FROM_ALL)
add_subdirectory(Vendor/glm)
add_subdirectory(Vendor/tracy)
add_subdirectory(Vendor/miniaudio)
add_subdirectory(Vendor/glfw)
add_subdirectory(Vendor/curl EXCLUDE_FROM_ALL)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	include(cmake/gnug-configuresubprojects.txt)
endif()

# organize VS folders
set_property(TARGET Vendor PROPERTY FOLDER "Vendor")
set_property(TARGET TracyClient PROPERTY FOLDER "Vendor")
set_property(TARGET glm PROPERTY FOLDER "Vendor")
set_property(TARGET miniaudio PROPERTY FOLDER "Vendor")
set_property(TARGET OpenGL PROPERTY FOLDER "Vendor")

if (MSVC)
	set_property(TARGET libcurl_object PROPERTY FOLDER "Vendor/Curl")
	set_property(TARGET libcurl_static PROPERTY FOLDER "Vendor/Curl")
endif()

set_property(TARGET Luau.Require PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.Compiler PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.Analysis PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.CLI.lib PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.CodeGen PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.Common PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.Config PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.Ast PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.VM PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET isocline PROPERTY FOLDER "Vendor/Luau")

set_property(TARGET glfw PROPERTY FOLDER "Vendor/GLFW3")
set_property(TARGET uninstall PROPERTY FOLDER "Vendor/GLFW3")
set_property(TARGET update_mappings PROPERTY FOLDER "Vendor/GLFW3")

# build engine after all libs
add_dependencies(PhoenixEngine
	Vendor
	OpenGL
	TracyClient
	glm
	Luau.Compiler
	Luau.Require
	Luau.VM
	libcurl
)

if (!PHX_HEADLESS)
	add_dependencies(PhoenixEngine glfw)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT PhoenixEngine)

set_target_properties(PhoenixEngine
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/x64/${PHX_BUILD_TYPE}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/x64/${PHX_BUILD_TYPE}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/x64/${PHX_BUILD_TYPE}"
)

set_target_properties(Vendor
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Vendor/x64/${PHX_BUILD_TYPE}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Vendor/x64/${PHX_BUILD_TYPE}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Vendor/x64/${PHX_BUILD_TYPE}"
)

# Get the full Git commit hash
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_COMMAND_RESULT
)

# Check if git command was successful
if (NOT GIT_COMMAND_RESULT EQUAL 0)
    message(WARNING "Unable to detect the current Engine Git commit hash.")
    set(GIT_COMMIT_HASH "Unknown")
endif()

message("The Git commit hash is ${GIT_COMMIT_HASH}")

set_property(SOURCE src/impl/Version.cpp PROPERTY COMPILE_DEFINITIONS "PHX_VERSION_HPP_ENGINE_GIT_COMMIT=\"${GIT_COMMIT_HASH}\"")
