# PhoenixEngine
garbaj spaghett

"Game Engine" that can load models (GLTF), materials (albedo+specular), textures, scripts (API W.I.P.), scenes (models+primitives+light sources+scripts), and can also do some color-only post-processing stuff (BROKEN RN). It also crashes upon exit right now, which is neat.

# Attributions
3rd-party code compiled directly with the Engine is located in the `vendor` folder
The following libraries are used:

* Luau, scripting - [@Roblox/Luau](github.com/Roblox/Luau/)
* SDL 2, window interface, OpenGL interface - [libsdl.org](libsdl.org/)
* Glad, OpenGL API - [glad.dav1d.de/](glad.dav1d.de/)
* GLM, matrix math - [@g-truc/glm](github.com/g-truc/glm/)
* nlohmann::json, JSON decoding/encoding for materials and scenes - [@nlohmann/json](https://github.com/nlohmann/json/)
* ImGui, GUIs - [@ocornut/ImGui](github.com/ocornut/imgui/)
* STB Image, image loading - [@nothings/stb](github.com/nothings/stb)
* Vulkan (UNUSED) - [Khronos Group](khronos.org/)
* OpenGL, rendering - [Khronos Group](khronos.org/)

The [OpenGL YouTube tutorials](youtube.com/watch?v=XpBGwZNyUh0&list=PLPaoO-vpZnumdcb4tZc4x5Q-v7CkrQ6M-) of [Victor Gordan](https://github.com/VictorGordan/) were referenced heavily during the creation of the rendering systems of this engine.
Initially, this started out as a 1-to-1 follow-along of what he did, but I rewrote the entire thing to work in a better "Game Engine"-style architecture, as the tutorial had a simpler Model Viewer architecture that was not suitable for a Game Engine.