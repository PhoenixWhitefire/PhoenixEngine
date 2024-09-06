# PhoenixEngine
garbaj spaghett

"Game Engine" that can load models (GLTF), materials (albedo+specular), textures, scripts (DISABLED RN), scenes, and can also do some color-only post-processing stuff.

# Attributions
3rd-party code compiled directly with the Engine is located in the `vendor` folder

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