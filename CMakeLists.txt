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

project(PhoenixEngine)

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

target_include_directories(PhoenixEngine PUBLIC
	"./"
	"src/decl"
	"Vendor"
	"Vendor/glm"
	"Vendor/SDL/include"
	"Vendor/imgui"
	"Vendor/tracy/public"
	"Vendor/luau/Common/include"
	"Vendor/luau/VM/include"
	"Vendor/luau/Compiler/include"
	"Vendor/luau/Ast/include"
)

set(PHX_BUILD_TYPE $<IF:$<CONFIG:Debug>,Debug,$<IF:$<CONFIG:Release>,Release,RelTracy>>)

target_compile_definitions(PhoenixEngine PRIVATE PHX_BUILD_TYPE="${PHX_BUILD_TYPE}")

add_compile_definitions($<$<CONFIG:Debug>:TRACY_ENABLE>)
add_compile_definitions($<$<CONFIG:Debug>:TRACY_FIBERS>)
add_compile_definitions($<$<CONFIG:Release>:NDEBUG>)
add_compile_definitions($<$<CONFIG:RelTracy>:NDEBUG>)
add_compile_definitions($<$<CONFIG:RelTracy>:TRACY_ENABLE>)
add_compile_definitions($<$<CONFIG:RelTracy>:TRACY_FIBERS>)

if (MSVC)
	include(cmake-msvc.txt)
else() # assume G++
	include(cmake-gnug.txt)
endif()

# remove unnecessary projects
option(LUAU_BUILD_CLI "Build CLI" OFF)
option(LUAU_BUILD_TESTS "Build tests" OFF)
option(SDL_TEST_LIBRARY "Build the SDL3_test library" OFF)

add_subdirectory(Vendor)
add_subdirectory(Vendor/luau)
add_subdirectory(Vendor/glm)
add_subdirectory(Vendor/SDL)
add_subdirectory(Vendor/tracy)

# organize vs folders

set_property(TARGET Vendor PROPERTY FOLDER "Vendor")
set_property(TARGET TracyClient PROPERTY FOLDER "Vendor")
set_property(TARGET glm PROPERTY FOLDER "Vendor")

set_property(TARGET Luau.RequireNavigator PROPERTY FOLDER "Vendor/Luau")
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
set_property(TARGET SDL3-shared PROPERTY FOLDER "Vendor/SDL")
set_property(TARGET SDL_uclibc PROPERTY FOLDER "Vendor/SDL")

# build engine after all static libs
add_dependencies(PhoenixEngine Vendor TracyClient glm Luau.Compiler Luau.Analysis Luau.RequireNavigator Luau.Require Luau.VM SDL3-shared SDL_uclibc)

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
