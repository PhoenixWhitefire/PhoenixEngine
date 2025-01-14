cmake_minimum_required(VERSION 3.29)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_VS_PLATFORM_TOOLSET v143)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Debug")
set_property(GLOBAL PROPERTY CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Debug")

project(PhoenixEngine)

set(PHX_OPTIONS)

list(APPEND PHX_OPTIONS /MP)
list(APPEND PHX_OPTIONS /DTRACY_ENABLE)

file(GLOB_RECURSE PHX_SRCS
	"src/decl/*.hpp"
	"src/decl/asset/*.hpp"
	"src/decl/datatype/*.hpp"
	"src/decl/gameobject/*.hpp"
	"src/decl/render/*.hpp"
	
	"src/impl/*.cpp"
	"src/impl/asset/*.cpp"
	"src/impl/datatype/*.cpp"
	"src/impl/gameobject/*.cpp"
	"src/impl/render/*.cpp"
)

add_executable(PhoenixEngine ${PHX_SRCS})

source_group(TREE src FILES ${PHX_SRCS})

set_target_properties(PhoenixEngine PROPERTIES OUTPUT_NAME PhoenixEngine)

target_compile_options(PhoenixEngine PRIVATE ${PHX_OPTIONS})

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
)

add_subdirectory(Vendor)
add_subdirectory(Vendor/luau)
add_subdirectory(Vendor/glm)
add_subdirectory(Vendor/SDL)
add_subdirectory(Vendor/tracy)

set_property(TARGET Vendor PROPERTY FOLDER "Vendor")
set_property(TARGET Luau.VM PROPERTY FOLDER "Vendor")
set_property(TARGET Luau.Compiler PROPERTY FOLDER "Vendor")
set_property(TARGET Luau.Ast PROPERTY FOLDER "Vendor")
set_property(TARGET glm PROPERTY FOLDER "Vendor")
set_property(TARGET SDL3-shared PROPERTY FOLDER "Vendor")
set_property(TARGET SDL_uclibc PROPERTY FOLDER "Vendor")
set_property(TARGET TracyClient PROPERTY FOLDER "Vendor")

set_property(TARGET isocline PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.Analysis PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.Analyze.CLI PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.Ast.CLI PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.Bytecode.CLI PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.CLI.lib PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.CLI.Test PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.CodeGen PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.Common PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.Compile.CLI PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.Config PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.Conformance PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.EqSat PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.Reduce.CLI PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.Repl.CLI PROPERTY FOLDER "NotRequired")
set_property(TARGET Luau.UnitTest PROPERTY FOLDER "NotRequired")
set_property(TARGET SDL3_test PROPERTY FOLDER "NotRequired")

target_link_libraries(PhoenixEngine
	opengl32.lib
	Vendor/Debug/Vendor
	Vendor/glm/glm/Debug/glm
	Vendor/luau/Debug/Luau.Ast
	Vendor/luau/Debug/Luau.Compiler
	Vendor/luau/Debug/Luau.VM
	Vendor/SDL/Debug/SDL_uclibc
	Vendor/SDL/Debug/SDL3
	Vendor/tracy/Debug/TracyClient
)

target_sources(PhoenixEngine PRIVATE README.md)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT PhoenixEngine)

set(PHX_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Debug/")
