# PhoenixEngine
garbaj spaghett

Personal "Game Engine" written in C++, that can:
* Import simple Models (`.gltf`)
* Execute Scripts (in [*Luau*](https://github.com/luau-lang/luau/))
* Render simple materials (Color + Metalness maps)
* Load/Save Scenes without losing data
* Do simple physics (dynamics and collisions w/ friction with AABBs, also has Raycasting for Scripts)
* Do some simple, color-only post-processing

# Branches
* `main` and `dev`, certified classics
* I'll commit to `dev` every once in a while and pull into `main` once in a Blue Moon

# Building
**Certain files are not present in the Git source tree! These are primarily non-essential game assets, but may include third-party library files and DLLs.**

* CMake
	* Minimum enforced version is `3.16.0`
	* I personally only tested with `3.30.2` (latest right now is `3.30.3`)
* C++ Standard `20` and C Standard `17`
* Visual Studio Platform Toolset `v143`
* Windows SDK `10.0`
* MSVC

**NOTICE**
*(28/09/2024)*
Don't try to use CMake. It won't work.
Manually setting up the project in Visual Studio will probably take less time than debugging the `CMakeList`s.

1. `git clone https://github.com/PhoenixWhitefire/PhoenixEngine --recursive`
	* `git clone https://github.com/PhoenixWhitefire/PhoenixEngine --recursive -b dev` for the `dev` branch	
	* (Or use the handy-dandy, big, green, `<> Code` button to download a `.ZIP` and extract it)
2. `cmake -S "./" -B "./"`
3. Open the resulting `.sln`, and you should have
	* `PhoenixEngine` as the startup project
	* A Solution Folder called "Vendor", containing:
		* `Luau.Ast`
		* `Luau.Common`
		* `Luau.Compiler`
		* `Luau.VM`
		* `Vendor`
4. Build the Solution. `Debug` does not work because of conflicts with Luau. Use `Release`.

# Attributions
3rd-party code is located in the `Vendor` directory and compiled into `lib/Vendor.lib` and statically linked with the Engine.

The following libraries are used:

* Luau, scripting - [@Roblox/Luau](https://github.com/Roblox/Luau/)
* SDL2, window interface, OpenGL interface - [libsdl.org](https://libsdl.org/)
* Glad, OpenGL API - [glad.dav1d.de/](https://glad.dav1d.de/)
* GLM, matrix math - [@g-truc/glm](https://github.com/g-truc/glm/)
* `nlohmann::json`, JSON decoding/encoding for materials and scenes - [@nlohmann/json](https://github.com/nlohmann/json/)
* Dear ImGui, User Interface - [@ocornut/ImGui](https://github.com/ocornut/imgui/)
* STB Image, image loading - [@nothings/stb](https://github.com/nothings/stb)
* OpenGL, Graphics API - [Khronos Group](https://khronos.org/)

The [OpenGL YouTube tutorials](https://youtube.com/watch?v=XpBGwZNyUh0&list=PLPaoO-vpZnumdcb4tZc4x5Q-v7CkrQ6M-) of [Victor Gordan](https://github.com/VictorGordan/) were referenced heavily during the creation of the rendering systems of this engine.
Initially, this started out as a 1-to-1 follow-along of what he did, but I rewrote the entire thing to work in a better "Game Engine"-style architecture, as the tutorial had a simpler Model Viewer architecture that was not suitable for a Game Engine.
