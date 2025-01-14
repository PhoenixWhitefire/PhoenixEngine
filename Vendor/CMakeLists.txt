cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_VS_PLATFORM_TOOLSET v143)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(Vendor)

set(PHX_OPTIONS)

list(APPEND PHX_OPTIONS /MP)

add_library(Vendor STATIC)

target_compile_options(Vendor PRIVATE ${PHX_OPTIONS})

target_include_directories(Vendor PUBLIC
	"./"
)

include(Sources.cmake)

add_subdirectory(glm)
add_subdirectory(luau)
add_subdirectory(SDL)
add_subdirectory(tracy)