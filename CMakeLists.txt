cmake_minimum_required(VERSION 3.29)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_VS_PLATFORM_TOOLSET v143)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Debug: Standard debug mode. Incremental linking. Tracy and ASan enabled
# Release: Optimizations, LTCG, no Incremental linking, Tracy and ASan disabled
# RelInstru: Optimizations, no LTCG, Incremental linking, Tracy and ASan enabled
set(CMAKE_CONFIGURATION_TYPES "Debug;RelInstru;Release")

project(PhoenixEngine)

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

target_compile_definitions(PhoenixEngine PRIVATE $<$<CONFIG:Debug>:TRACY_ENABLE>)
target_compile_definitions(PhoenixEngine PRIVATE $<$<CONFIG:RelInstru>:TRACY_ENABLE>)

target_compile_options(PhoenixEngine PRIVATE $<$<CONFIG:RelInstru>:/O2 /Ob2>)
target_compile_options(PhoenixEngine PRIVATE $<$<CONFIG:Release>:/GL>)
target_compile_options(PhoenixEngine PRIVATE /MP /W4)

target_compile_options(PhoenixEngine PRIVATE $<$<CONFIG:Debug>:/fsanitize=address /Od>)
target_compile_options(PhoenixEngine PRIVATE $<$<CONFIG:RelInstru>:/fsanitize=address /O2 /Ob2 /Zi>)

target_compile_options(PhoenixEngine PRIVATE $<$<CONFIG:Release>:/GL>)

add_compile_options($<$<CONFIG:RelInstru>:/Zi>)
add_compile_options($<$<CONFIG:Release>:/GL>)
add_link_options($<$<CONFIG:RelInstru>:/DEBUG>)
add_link_options($<$<CONFIG:RelInstru>:/INCREMENTAL:NO>)
add_link_options($<$<CONFIG:Debug>:/INCREMENTAL:NO>)

target_link_options(PhoenixEngine PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL:NO>)
target_link_options(PhoenixEngine PRIVATE $<$<CONFIG:RelInstru>:/INCREMENTAL:NO /DEBUG>)
target_link_options(PhoenixEngine PRIVATE $<$<CONFIG:Release>:/LTCG>)

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

set(PHX_BUILD_TYPE $<IF:$<CONFIG:Debug>,Debug,$<IF:$<CONFIG:Release>,Release,RelInstru>>)
set(PHX_TRACY_LIB_PATH Vendor/tracy/${PHX_BUILD_TYPE}/TracyClient)

target_compile_options(PhoenixEngine PRIVATE /DPHX_BUILD_TYPE="${PHX_BUILD_TYPE}")

target_link_libraries(PhoenixEngine
	opengl32.lib
	Vendor/${PHX_BUILD_TYPE}/Vendor
	Vendor/glm/glm/${PHX_BUILD_TYPE}/glm
	Vendor/luau/${PHX_BUILD_TYPE}/Luau.Ast
	Vendor/luau/${PHX_BUILD_TYPE}/Luau.Compiler
	Vendor/luau/${PHX_BUILD_TYPE}/Luau.VM
	Vendor/SDL/${PHX_BUILD_TYPE}/SDL_uclibc
	Vendor/SDL/${PHX_BUILD_TYPE}/SDL3
	
	# too much work
	${PHX_TRACY_LIB_PATH}
	# IF:$<$<CONFIG:Debug> ${PHX_TRACY_LIB_PATH} $<$<CONFIG:RelInstru>:${PHX_TRACY_LIB_PATH}>
)

target_sources(PhoenixEngine PRIVATE README.md)

# /INCREMENTAL warning with ASan
add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
add_compile_options($<$<CONFIG:Debug>:/Od>)
add_compile_options($<$<CONFIG:RelInstru>:/fsanitize=address>)
add_compile_options($<$<CONFIG:RelInstru>:/O2>)
add_compile_options($<$<CONFIG:RelInstru>:/Ob2>)

add_compile_options($<$<CONFIG:Release>:/GL>)

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

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT PhoenixEngine)

# couldn't be bothered to get $<TARGET_RUNTIME_DLLS:PhoenixEngine> to work
add_custom_command(TARGET PhoenixEngine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:PhoenixEngine> "Vendor/SDL/${PHX_BUILD_TYPE}/SDL3.dll"
  COMMAND_EXPAND_LISTS
)
