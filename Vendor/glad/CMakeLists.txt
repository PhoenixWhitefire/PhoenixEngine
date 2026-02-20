cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_VS_PLATFORM_TOOLSET v143)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(OpenGL)

set(GL_SRCS
	"gl.h"
	"gl.c"
	"KHR/khrplatform.h"
)

add_library(OpenGL STATIC ${GL_SRCS})

source_group(TREE ./ FILES ${GL_SRCS})

target_include_directories(OpenGL PUBLIC
	"./"
    "../"
)
