cmake_minimum_required(VERSION 3.29)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_VS_PLATFORM_TOOLSET v143)
set(CMAKE_CXX_STANDARD_REQUIRED True)

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
target_compile_definitions(PhoenixEngine PRIVATE $<$<CONFIG:RelTracy>:TRACY_ENABLE NDEBUG>)

target_compile_options(PhoenixEngine PRIVATE $<$<CONFIG:RelTracy>:/O2 /Ob2 /GL>)
target_compile_options(PhoenixEngine PRIVATE $<$<CONFIG:Release>:/O2 /Ob2 /GL>)
target_compile_options(PhoenixEngine PRIVATE $<$<CONFIG:Debug>:/Od /Ob0>)
target_compile_options(PhoenixEngine PRIVATE /MP /W4)

target_compile_options(PhoenixEngine PRIVATE $<$<CONFIG:Debug>:/fsanitize=address /Od>)

target_link_options(PhoenixEngine PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL:NO>)
target_link_options(PhoenixEngine PRIVATE $<$<CONFIG:RelTracy>:/LTCG>)
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

set(PHX_BUILD_TYPE $<IF:$<CONFIG:Debug>,Debug,$<IF:$<CONFIG:Release>,Release,RelTracy>>)
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
	
	# too much of a waste of my time to get the `IF` expression to work, link regardless
	${PHX_TRACY_LIB_PATH}
	
	# IF:$<$<CONFIG:Debug> ${PHX_TRACY_LIB_PATH} $<$<CONFIG:RelTracy>:${PHX_TRACY_LIB_PATH}>
)

target_sources(PhoenixEngine PRIVATE README.md WindowsExeMetadata.rct)

add_compile_definitions($<$<CONFIG:RelTracy>:NDEBUG>)

add_compile_options($<$<CONFIG:RelTracy>:/GL>)
add_compile_options($<$<CONFIG:RelTracy>:/O2>)
add_compile_options($<$<CONFIG:RelTracy>:/Ob2>)
add_compile_options($<$<CONFIG:Release>:/GL>)
add_compile_options($<$<CONFIG:Release>:/O2>)
add_compile_options($<$<CONFIG:Release>:/Ob2>)
add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
add_compile_options($<$<CONFIG:Debug>:/Od>)
add_compile_options($<$<CONFIG:Debug>:/DEBUG>)

add_compile_options(/MP)
# incremental warning with ASAN
add_link_options($<$<CONFIG:Debug>:/INCREMENTAL:NO>)

# incremental warning with LTCG
add_link_options($<$<CONFIG:RelTracy>:/INCREMENTAL:NO>)
add_link_options($<$<CONFIG:RelTracy>:/LTCG>)
add_link_options($<$<CONFIG:Release>:/INCREMENTAL:NO>)
add_link_options($<$<CONFIG:Release>:/LTCG>)

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

set_target_properties(PhoenixEngine
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/x64/${PHX_BUILD_TYPE}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/x64/${PHX_BUILD_TYPE}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/x64/${PHX_BUILD_TYPE}"
)

# couldn't be bothered to get $<TARGET_RUNTIME_DLLS:PhoenixEngine> to work
add_custom_command(TARGET PhoenixEngine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:PhoenixEngine> "Vendor/SDL/${PHX_BUILD_TYPE}/SDL3.dll"
  COMMAND_EXPAND_LISTS
)
