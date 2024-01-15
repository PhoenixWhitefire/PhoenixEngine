// TODO: cleanup code!

#include<MapLoader.hpp>

#include"gameobject/MeshObject.hpp"
#include"gameobject/Light.hpp"

Vector3 GetVector3FromJSON(nlohmann::json JSON)
{
	int Index = -1;

	Vector3 Vec3;

	try
	{
		Vec3 = Vector3(
			JSON[++Index],
			JSON[++Index],
			JSON[++Index]
		);
	}
	catch (nlohmann::json::type_error TErr)
	{
		Debug::Log("Could not read Vector3!\nError: '" + std::string(TErr.what()) + "'");
	}

	return Vec3;
}

void MapLoader::LoadMapIntoObject(const char* MapFilePath, std::shared_ptr<GameObject> MapParent)
{
	bool MapExists = true;
	std::string FileContents = FileRW::ReadFile(MapFilePath, &MapExists);

	//TODO fix de-allocation issues, std::shared_ptr has caused the majority of runtime access violations ever for the engine
	// pwf 2022
	// ULTRA EXTREME 1000% TODO: requires nuclear-level refactoring all throughout engine code, if only I wasn't so stupid before :(
	// Maybe void* is better?
	// idk, TODO find better solution future me
	std::vector<std::shared_ptr<GameObject>>& LoadedObjects = *new std::vector<std::shared_ptr<GameObject>>(0);

	if (!MapExists)
	{
		std::string ErrorMessage = std::vformat("Map file not found: '{}'", std::make_format_args(MapFilePath));

		Debug::Log(ErrorMessage);

		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "map load error", ErrorMessage.c_str(), SDL_GetGrabbedWindow());

		return;
	}

	if (FileContents == "")
	{
		std::string ErrorMessage = std::vformat("Empty map file: '{}'", std::make_format_args(MapFilePath));

		Debug::Log(ErrorMessage);

		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "map load error", ErrorMessage.c_str(), SDL_GetGrabbedWindow());

		return;
	}

	nlohmann::json JSONData;

	try
	{
		JSONData = nlohmann::json::parse(FileContents);
	}
	catch (nlohmann::json::exception e)
	{
		SDL_ShowSimpleMessageBox(0, "Engine error", (std::vformat("Could not parse map data from file '{}': {}", std::make_format_args(MapFilePath, e.what()))).c_str(), SDL_GetGrabbedWindow());
		throw(std::vformat("Could not parse map data from file '{}': {}", std::make_format_args(MapFilePath, e.what())));

		return;
	}

	nlohmann::json PartsNode = JSONData["parts"];
	nlohmann::json ModelsNode = JSONData.find("props") != JSONData.end() ? JSONData["props"] : nlohmann::json({});
	nlohmann::json LightsNode = JSONData["lights"];

	std::vector<std::string> LoadedTexturePaths;
	std::vector<GLuint*> LoadedTextures;

	std::vector<Vertex> ev(0);
	std::vector<GLuint> ei(0);

	Mesh EmptyMesh = Mesh(ev, ei);

	for (unsigned int Index = 0; Index < ModelsNode.size(); Index++)
	{
		nlohmann::json PropObject;

		try
		{
			PropObject = ModelsNode[Index];
		}
		catch (nlohmann::json::type_error e)
		{
			throw(std::vformat("Failed to decode map data: {}", std::make_format_args(e.what())));
		}

		std::string ModelPath = PropObject["path"];

		Vector3 Position = PropObject.find("position") != PropObject.end() ? GetVector3FromJSON(PropObject["position"]) : Vector3::ZERO;
		Vector3 Orientation = PropObject.find("orient") != PropObject.end() ? GetVector3FromJSON(PropObject["orient"]) : Vector3::ZERO;
		Vector3 Size = PropObject.find("size") != PropObject.end() ? GetVector3FromJSON(PropObject["size"]) : Vector3(1.0f, 1.0f, 1.0f);

		auto& Model = EngineObject::Get()->LoadModelAsMeshesAsync(ModelPath.c_str(), Size, Position);

		if (PropObject.find("name") != PropObject.end())
		{
			if (Model.size() == 0)
				Debug::Log(std::vformat("Model was given name '{}' in map file '{}', but has no meshes!", std::make_format_args("<cast error>", MapFilePath)));
			else // TODO: all meshes should be given the name, plus a number probably
				Model[0]->Name = PropObject["name"];
		}

		if (PropObject.find("facecull") != PropObject.end())
		{
			if ((std::string)PropObject["facecull"] == (std::string)"none")
				std::dynamic_pointer_cast<Object_Mesh>(Model[0])->GetRenderMesh()->CulledFace = FaceCullingMode::None;
			else if ((std::string)PropObject["facecull"] == (std::string)"front")
				std::dynamic_pointer_cast<Object_Mesh>(Model[0])->GetRenderMesh()->CulledFace = FaceCullingMode::FrontFace;
			else if ((std::string)PropObject["facecull"] == (std::string)"back")
				std::dynamic_pointer_cast<Object_Mesh>(Model[0])->GetRenderMesh()->CulledFace = FaceCullingMode::BackFace;
			else
				std::dynamic_pointer_cast<Object_Mesh>(Model[0])->GetRenderMesh()->CulledFace = FaceCullingMode::BackFace;
		}

		auto prop_3d = std::dynamic_pointer_cast<Object_Base3D>(Model[0]);

		if (PropObject.find("has_transparency") != PropObject.end())
			std::dynamic_pointer_cast<Object_Mesh>(Model[0])->HasTransparency = true;

		prop_3d->ComputePhysics = PropObject.value("computePhysics", 0) == 1 ? true : false;

		if (prop_3d->ComputePhysics)
		{
			Debug::Log(std::vformat("'{}' has physics! (computePhysics set to 1 in the .WORLD file)", std::make_format_args(prop_3d->Name)));
		}
	}

	for (unsigned int Index = 0; Index < PartsNode.size(); Index++)
	{
		nlohmann::json Object = PartsNode[Index];

		int TextureSlot = 0;

		if (Object["type"] == "MeshPart")
		{
			std::shared_ptr<GameObject> NewObject = (GameObjectFactory::CreateGameObject("MeshPart"));

			std::shared_ptr<Object_Base3D> Object3D = std::dynamic_pointer_cast<Object_Base3D>(NewObject);

			std::shared_ptr<Object_Mesh> MeshObject = std::dynamic_pointer_cast<Object_Mesh>(NewObject);

			if (Object.find("name") != Object.end())
			{
				NewObject->Name = Object["name"];
				Object3D->Name = Object["name"];
				MeshObject->Name = Object["name"];
			}

			Vector3 Position = GetVector3FromJSON(Object["position"]);
			Vector3 Orientation;

			if (Object.find("orient") != Object.end())
			{
				Orientation = GetVector3FromJSON(Object["orient"]);
				/*
				Orientation = Vector3(
					Orientation.Y,
					Orientation.Z,
					Orientation.X
				);
				*/
			}

			Object3D->Matrix = glm::translate(Object3D->Matrix, (glm::vec3)Position);

			Object3D->Matrix = glm::rotate(Object3D->Matrix, glm::radians((float)Orientation.X), glm::vec3(1.0f, 0.0f, 0.0f));
			Object3D->Matrix = glm::rotate(Object3D->Matrix, glm::radians((float)Orientation.Y), glm::vec3(0.0f, 1.0f, 0.0f));
			Object3D->Matrix = glm::rotate(Object3D->Matrix, glm::radians((float)Orientation.Z), glm::vec3(0.0f, 0.0f, 1.0f));

			if (Object.find("color") != Object.end())
			{
				Vector3 ColorVect = GetVector3FromJSON(Object["color"]);

				Object3D->ColorRGB = Color(ColorVect.X, ColorVect.Y, ColorVect.Z);
			}

			MeshObject->Size = GetVector3FromJSON(Object["size"]);

			MeshObject->SetRenderMesh(*BaseMeshes::Cube());

			NewObject->SetParent(MapParent);

			if (Object.find("material") != Object.end())
			{
				Material* MeshMaterial = MaterialGetter::Get().GetMaterial(Object["material"]);

				Object3D->Material = Object["material"];

				MeshObject->Textures.push_back(MeshMaterial->Diffuse);

				if (MeshMaterial->HasSpecular)
					MeshObject->Textures.push_back(MeshMaterial->Specular);
			}
			else if (Object.find("textures") != Object.end())
			{

			}

			Object3D->ComputePhysics = Object.value("computePhysics", 0) == 1 ? true : false;

			if (Object3D->ComputePhysics)
			{
				Debug::Log(std::vformat("'{}' has physics! (computePhysics set to 1 in the .WORLD file)", std::make_format_args(Object3D->Name)));
			}

			LoadedObjects.push_back(NewObject);
		}

		std::string NewTitle = std::to_string(Index) + "/" + std::to_string(PartsNode.size()) + " - " + std::to_string((float)Index / (float)PartsNode.size());
	}

	for (unsigned int LightIndex = 0; LightIndex < LightsNode.size(); LightIndex++)
	{
		nlohmann::json LightObject = LightsNode[LightIndex];

		std::string LightType = LightObject["type"];

		std::shared_ptr<GameObject> Object = (GameObjectFactory::CreateGameObject(LightType));
		std::shared_ptr<Object_Light> Light = std::dynamic_pointer_cast<Object_Light>(Object);

		Light->Position = GetVector3FromJSON(LightObject["position"]);

		Vector3 LightColor = GetVector3FromJSON(LightObject["color"]);

		Light->LightColor = Color(LightColor.X, LightColor.Y, LightColor.Z);

		if (LightType == "PointLight")
		{
			std::shared_ptr<Object_PointLight> Pointlight = std::dynamic_pointer_cast<Object_PointLight>(Object);
			Pointlight->Range = LightObject["range"];
		}

		if (LightType == "DirectionalLight")
		{
			printf("direclight??\n");
			std::shared_ptr<Object_DirectionalLight> DirectLight = std::dynamic_pointer_cast<Object_DirectionalLight>(Object);
			DirectLight->Position = GetVector3FromJSON(LightObject["position"]);
			DirectLight->LightColor = Color(LightColor.X, LightColor.Y, LightColor.Z);
		}

		Object->SetParent(MapParent);

		LoadedObjects.push_back(Object);
	}

	Debug::Log(std::vformat("Loaded {} objects from map file {}", std::make_format_args(LoadedObjects.size(), MapFilePath)));
}
