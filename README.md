# PhoenixEngine
*garbaj spaghett*

<hr>

Personal "Game Engine" written in C++, that can:
* Render scenes, i.e. multiple objects with different materials (Color, Metallic-Roughness, Emission and Normal maps), with GPU instancing
* Import simple Models (`.gltf`/`.glb`)
* Execute [*Luau*](https://github.com/luau-lang/luau/) scripts w/ per-frame callbacks
* Load/Save Scenes
* Do simple physics (dynamics and collisions w/ friction with AABBs, also has Raycasting for Scripts)
* Do some simple, color-only post-processing (really weird Bloom, but a kinda nice blur vignette)

Developed on Windows with Visual Studio Community 2022. Not tested on other platforms, however there shouldn't be anything *too* incompatible, all libraries used are explicitly cross-platform.

# Building

* CMake
	* Minimum enforced version is `3.29`
	* I personally only tested with `3.30.2`
* C++ Standard `20` and C Standard `17`
* Visual Studio Platform Toolset `v143`
* Windows SDK `10.0`
* MSVC

1. `git clone https://github.com/PhoenixWhitefire/PhoenixEngine --recursive`, or just use the `Code <>` button
2. `cmake -B "./"` in the root directory
3. Open the resulting `PhoenixEngine.sln`, and you should have:
	* `PhoenixEngine` as the startup project
	* A Solution Folder called "Vendor", containing the following subprojects:
		* `glm`
		* `Luau.Ast`
		* `Luau.Common`
		* `Luau.Compiler`
		* `Luau.VM`
		* `SDL_uclibc`
		* `SDL3-shared`
		* `TracyClient`
		* `Vendor`
	* A Solution Folder called "NotRequired". Right-click and `Remove` to avoid unnecessary compilation
4. Build the Solution in either:
	* `Debug`: Standard Debug build, no optimizations, Address Sanitizer and Tracy
	* `RelInstru`: Debug with compiler optimizations, Address Sanitizer and Tracy
	* `Release`: All optimizations, including Linker ones, no Address Sanitizer or Tracy
6. Run.

# Attributions

All 3rd-party code and submodules are located in the `Vendor` directory.

The following libraries are used:

* Luau, scripting - [@Roblox/Luau](https://github.com/Roblox/Luau/)
* SDL3, window interface, OpenGL interface - [libsdl.org](https://libsdl.org/)
* Glad, OpenGL API - [glad.dav1d.de/](https://glad.dav1d.de/)
* GLM, matrix math - [@g-truc/glm](https://github.com/g-truc/glm/)
* `nlohmann::json`, JSON decoding/encoding for various asset types - [@nlohmann/json](https://github.com/nlohmann/json/)
* Dear ImGui, user interface - [@ocornut/ImGui](https://github.com/ocornut/imgui/)
* `ImGuiFD`, file dialogs - [@Julianiolo/ImGuiFD](https://github.com/Julianiolo/ImGuiFD)
* STB Image, image loading - [@nothings/stb](https://github.com/nothings/stb)
* OpenGL, graphics API - [Khronos Group](https://khronos.org/)
* Tracy, profiling - [@wolfpld/Tracy](https://github.com/wolfpld/tracy)

The [OpenGL YouTube tutorials](https://youtube.com/watch?v=XpBGwZNyUh0&list=PLPaoO-vpZnumdcb4tZc4x5Q-v7CkrQ6M-) of [Victor Gordan](https://github.com/VictorGordan/) (who I occasionally make fun of in the code comments) were referenced heavily during the creation of the rendering systems of this engine.
Initially, this started out as a 1-to-1 follow-along of what he did, but I rewrote the entire thing to work in a better "Game Engine"-style architecture, as the tutorial had a simpler Model Viewer architecture that was not suitable for a Game Engine.
