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
# Release: Optimizations, LTCG, no Incremental linking, Tracy and ASan disabled
# RelTracy: Exact same as `Release`, but with Tracy instrumentation enabled
# My plan is for the standalone Editor to use `RelTracy` for easy performance debugging,
# but for distribution binaries to be `Release` for best performance (because Tracy introduces it's own overhead)
# 19/01/2025
set(CMAKE_CONFIGURATION_TYPES "Debug;RelTracy;Release")

project(PhoenixEngine LANGUAGES C CXX)

file(GLOB_RECURSE PHX_SRCS
	"src/decl/*.hpp"
	"src/impl/*.cpp"
)

add_executable(PhoenixEngine ${PHX_SRCS})

source_group(TREE src FILES ${PHX_SRCS})
target_sources(PhoenixEngine PRIVATE README.md phoenix.conf)

set_target_properties(PhoenixEngine PROPERTIES OUTPUT_NAME PhoenixEngine)

target_compile_definitions(PhoenixEngine PRIVATE $<$<CONFIG:Debug>:TRACY_ENABLE TRACY_FIBERS>)
target_compile_definitions(PhoenixEngine PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_compile_definitions(PhoenixEngine PRIVATE $<$<CONFIG:RelTracy>:NDEBUG TRACY_ENABLE TRACY_FIBERS>)
target_compile_definitions(PhoenixEngine PRIVATE PHX_TARGET_PLATFORM="${CMAKE_SYSTEM_NAME}")

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
)

set(PHX_BUILD_TYPE $<IF:$<CONFIG:Debug>,Debug,$<IF:$<CONFIG:Release>,Release,RelTracy>>)

target_compile_definitions(PhoenixEngine PRIVATE PHX_BUILD_TYPE="${PHX_BUILD_TYPE}")

if (MSVC)
	target_compile_options(PhoenixEngine PRIVATE /wd4244)
endif()

target_link_libraries(PhoenixEngine PRIVATE
	Vendor
	glm
	miniaudio
	glfw
	$<IF:$<CONFIG:Release>,,TracyClient> # TODO: figure out how to not build this if we dont link it

	Luau.Compiler
	Luau.Ast
	Luau.VM
	Luau.Require
	Luau.Config
)

# not letting me put multiple definitions in the same expression waaa
add_compile_definitions($<$<CONFIG:Debug>:TRACY_ENABLE>)
add_compile_definitions($<$<CONFIG:Debug>:TRACY_FIBERS>)
add_compile_definitions($<$<CONFIG:Release>:NDEBUG>)
add_compile_definitions($<$<CONFIG:RelTracy>:NDEBUG>)
add_compile_definitions($<$<CONFIG:RelTracy>:TRACY_ENABLE>)
add_compile_definitions($<$<CONFIG:RelTracy>:TRACY_FIBERS>)

if (MSVC)
	include(cmake/cmake-msvc.txt)
	target_compile_definitions(PhoenixEngine PRIVATE PHX_TARGET_COMPILER="MSVC")
else() # assume G++
	include(cmake/cmake-gnug.txt)
	target_compile_definitions(PhoenixEngine PRIVATE PHX_TARGET_COMPILER="G++")
	target_link_libraries(PhoenixEngine PRIVATE X11) # audible sigh when i checked `imgui_impl_glfw.cpp` line 1266
endif()

# remove unnecessary projects
option(LUAU_BUILD_CLI "Build CLI" OFF)
option(LUAU_BUILD_TESTS "Build tests" OFF)
option(MINIAUDIO_NO_EXTRA_NODES                "Do not build extra node graph nodes" ON)

add_subdirectory(Vendor)
add_subdirectory(Vendor/luau)
add_subdirectory(Vendor/glm)
add_subdirectory(Vendor/tracy)
add_subdirectory(Vendor/miniaudio)
add_subdirectory(Vendor/glfw)

# organize VS folders
set_property(TARGET Vendor PROPERTY FOLDER "Vendor")
set_property(TARGET TracyClient PROPERTY FOLDER "Vendor")
set_property(TARGET glm PROPERTY FOLDER "Vendor")
set_property(TARGET miniaudio PROPERTY FOLDER "Vendor")

set_property(TARGET Luau.Require PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.Compiler PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.Analysis PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.CLI.lib PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.CodeGen PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.Common PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.Config PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.EqSat PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.Ast PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET Luau.VM PROPERTY FOLDER "Vendor/Luau")
set_property(TARGET isocline PROPERTY FOLDER "Vendor/Luau")

set_property(TARGET glfw PROPERTY FOLDER "Vendor/GLFW3")
set_property(TARGET uninstall PROPERTY FOLDER "Vendor/GLFW3")
set_property(TARGET update_mappings PROPERTY FOLDER "Vendor/GLFW3")

# build engine after all libs
add_dependencies(PhoenixEngine
	Vendor
	TracyClient
	glm
	Luau.Compiler
	Luau.Analysis
	Luau.Require
	Luau.VM
	glfw
)

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
