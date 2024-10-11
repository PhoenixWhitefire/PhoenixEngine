cmake_minimum_required(VERSIOBN 3.30)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_VS_PLATFORM_TOOLSET v143)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(PhoenixEngine)

target_include_directories(PhoenixEngine PUBLIC
				"${PROJECT_DIR}",
				"${PROJECT_DIR}/src",
				"${PROJECT_DIR}/src/include"
			)

add_executable(PhoenixEngine Main.cpp)

set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT PhoenixEngine)