cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_VS_PLATFORM_TOOLSET v143)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(Vendor)

set(VEND_SRCS
	"stb/stb_image.h"
	"stb.cpp"
	
	"imgui/imgui.cpp"
	"imgui/imgui.h"
	"imgui/imgui_internal.h"
	"imgui/imgui_demo.cpp"
	"imgui/imgui_draw.cpp"
	"imgui/imconfig.h"
	"imgui/imgui_tables.cpp"
	"imgui/imgui_widgets.cpp"
	"imgui/backends/imgui_impl_opengl3.h"
	"imgui/backends/imgui_impl_glfw.h"
	"imgui/backends/imgui_impl_opengl3.cpp"
	"imgui/backends/imgui_impl_glfw.cpp"
	"imgui/misc/cpp/imgui_stdlib.h"
	"imgui/misc/cpp/imgui_stdlib.cpp"

	"ImGuiColorTextEdit/TextEditor.cpp"

	"tinyfd/tinyfiledialogs.c"
)

add_library(Vendor STATIC ${VEND_SRCS})

source_group(TREE ./ FILES ${VEND_SRCS})

target_include_directories(Vendor PUBLIC
	"./"
	"imgui"
	"ImGuiColorTextEdit"
	"glfw/include"
	"tinyfd"
)

add_subdirectory(glad)
add_dependencies(Vendor OpenGL)
