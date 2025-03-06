# PhoenixEngine
*✨ garbaj spaghett ✨*

<hr>

Personal "Game Engine" written in C++, that can:
* Render scenes, i.e. multiple objects with different materials (Color, Metallic-Roughness, Emission and Normal maps), with GPU instancing
* Import simple Models (`.gltf`/`.glb`)
* Execute [*Luau*](https://github.com/luau-lang/luau/) scripts w/ per-frame callbacks
* Load/Save Scenes
* Do simple physics (dynamics and collisions w/ friction with AABBs, also has Raycasting for Scripts)
* Do some simple, color-only post-processing (really weird Bloom, but a kinda nice blur vignette)

Developed primary on and for Windows with Visual Studio Community 2022 with MSVC, and occasionally I'll make sure it works on Ubuntu with G++.

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
    * On Linux/with Ninja, use `cmake -B "./" -G "Ninja Multi-Config"`
    * When using Visual Studio Code, set the Generator to "Ninja Multi-Config", and change the "Build Directory" setting to just `${workspaceFolder}`
    
3. Open the resulting `PhoenixEngine.sln`, and you should have:
	* `PhoenixEngine` as the startup project
	* A Solution Folder called "Vendor", containing a few sub-projects and an `SDL` and `Luau` sub-folder
4. Build the Solution in either:
	* `Debug`: Standard Debug build, no optimizations, Address Sanitizer and Tracy
	* `Release`: All optimizations, including Linker ones, no Address Sanitizer or Tracy
	* `RelTracy`: Same as `Release`, but with Tracy instrumentation enabled, as that carries it's own overhead
6. Run.
7. (Optional) I have not configured the Tracy Profiler standalone application to build along with the rest of the Engine, you will need to build it manually. The "Start Profiling" button in the Info widget, as well as the `-tracyim` launch argument, all assume you have built the Profiler yourself and that it is in the expected directory. You can do this with the following commands:
    * `cd Vendor/tracy/profiler`
    * `cmake -B build -G Ninja` (You can omit `-G Ninja` if you're OK with it using Make)
    * `cmake --build build --config Release`
    
    By the end, you should have a binary at the location `Vendor/tracy/profiler/build/tracy-profiler`.

# Attributions

All 3rd-party code and submodules are located in the `Vendor` directory.

The following libraries are used:

* Luau, scripting - [@Roblox/Luau](https://github.com/Roblox/Luau/)
* SDL3, window interface, OpenGL interface - [libsdl.org](https://libsdl.org/)
* Glad, OpenGL API - [glad.dav1d.de/](https://glad.dav1d.de/)
* GLM, matrix math - [@g-truc/glm](https://github.com/g-truc/glm/)
* `nlohmann::json`, JSON decoding/encoding for various asset types - [@nlohmann/json](https://github.com/nlohmann/json/)
* Dear ImGui, user interface - [@ocornut/ImGui](https://github.com/ocornut/imgui/)
* STB Image, image loading - [@nothings/stb](https://github.com/nothings/stb)
* OpenGL, graphics API - [Khronos Group](https://khronos.org/)
* Tracy, profiling - [@wolfpld/Tracy](https://github.com/wolfpld/tracy)

The [OpenGL YouTube tutorials](https://youtube.com/watch?v=XpBGwZNyUh0&list=PLPaoO-vpZnumdcb4tZc4x5Q-v7CkrQ6M-) of [Victor Gordan](https://github.com/VictorGordan/) (who I occasionally make fun of in the code comments) were referenced heavily during the creation of the rendering systems of this engine.
Initially, this started out as a 1-to-1 follow-along of what he did, but I rewrote the entire thing to work in a better "Game Engine"-style architecture, as the tutorial had a simpler Model Viewer architecture that was not suitable for a Game Engine.
