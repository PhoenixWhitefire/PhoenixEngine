target_sources(PhoenixEngine PRIVATE
	src/decl/asset/MaterialManager.hpp
	src/decl/asset/MeshProvider.hpp
	src/decl/asset/ModelImporter.hpp
	src/decl/asset/PrimitiveMeshes.hpp
	src/decl/asset/SceneFormat.hpp
	src/decl/asset/TextureManager.hpp
	
	src/decl/datatype/Color.hpp
	src/decl/datatype/Event.hpp
	src/decl/datatype/GameObject.hpp
	src/decl/datatype/Mesh.hpp
	src/decl/datatype/ValueSequence.hpp
	src/decl/datatype/Vector2.hpp
	src/decl/datatype/Vector3.hpp
	
	src/decl/gameobject/Attachment.hpp
	src/decl/gameobject/Base3D.hpp
	src/decl/gameobject/Camera.hpp
	src/decl/gameobject/DataModel.hpp
	src/decl/gameobject/Example.hpp
	src/decl/gameobject/GameObjects.hpp
	src/decl/gameobject/Light.hpp
	src/decl/gameobject/MeshObject.hpp
	src/decl/gameobject/Model.hpp
	src/decl/gameobject/ParticleEmitter.hpp
	src/decl/gameobject/Primitive.hpp
	src/decl/gameobject/Script.hpp
	src/decl/gameobject/ScriptEngine.hpp
	src/decl/gameobject/Sound.hpp
	src/decl/gameobject/Workspace.hpp
	
	src/decl/render/GpuBuffers.hpp
	src/decl/render/GraphicsAbstractionLayer.hpp
	src/decl/render/Renderer.hpp
	src/decl/render/RendererScene.hpp
	src/decl/render/ShaderManager.hpp
	
	src/decl/AnimationService.hpp
	src/decl/CameraController.hpp
	src/decl/Editor.hpp
	src/decl/Engine.hpp
	src/decl/EntityComponent.hpp
	src/decl/FileRW.hpp
	src/decl/GlobalJsonConfig.hpp
	src/decl/IntersectionLib.hpp
	src/decl/Log.hpp
	src/decl/PhysicsEngine.hpp
	src/decl/Profiler.hpp
	src/decl/Reflection.hpp
	src/decl/ThreadManager.hpp
	src/decl/UserInput.hpp
	src/decl/Utilities.hpp
	
	
	src/impl/asset/MaterialManager.cpp
	src/impl/asset/MeshProvider.cpp
	src/impl/asset/ModelImporter.cpp
	src/impl/asset/PrimitiveMeshes.cpp
	src/impl/asset/SceneFormat.cpp
	src/impl/asset/TextureManager.cpp
	
	src/impl/datatype/Color.cpp
	src/impl/datatype/GameObject.cpp
	src/impl/datatype/Vector2.cpp
	src/impl/datatype/Vector3.cpp
	
	src/impl/gameobject/Attachment.cpp
	src/impl/gameobject/Base3D.cpp
	src/impl/gameobject/Camera.cpp
	src/impl/gameobject/DataModel.cpp
	src/impl/gameobject/Example.cpp
	src/impl/gameobject/Light.cpp
	src/impl/gameobject/MeshObject.cpp
	src/impl/gameobject/Model.cpp
	src/impl/gameobject/ParticleEmitter.cpp
	src/impl/gameobject/Primitive.cpp
	src/impl/gameobject/Script.cpp
	src/impl/gameobject/ScriptEngine.cpp
	src/impl/gameobject/Sound.cpp
	src/impl/gameobject/Workspace.cpp
	
	src/impl/render/GpuBuffers.cpp
	src/impl/render/GraphicsAbstractionLayer.cpp
	src/impl/render/Renderer.cpp
	src/impl/render/ShaderManager.cpp
	
	src/impl/AnimationService.cpp
	src/impl/CameraController.cpp
	src/impl/Editor.cpp
	src/impl/Engine.cpp
	src/impl/EntityComponent.cpp
	src/impl/FileRW.cpp
	src/impl/GlobalJsonConfig.cpp
	src/impl/IntersectionLib.cpp
	src/impl/Log.cpp
	src/impl/PhysicsEngine.cpp
	src/impl/Profiler.cpp
	src/impl/Reflection.cpp
	src/impl/ThreadManager.cpp
	src/impl/UserInput.cpp
	src/impl/Utilities.cpp
	
	src/impl/Main.cpp
)
