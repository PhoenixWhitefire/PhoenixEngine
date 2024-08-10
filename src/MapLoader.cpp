// TODO: cleanup code!

#include<nljson.h>
#include<SDL2/SDL_messagebox.h>
#include<SDL2/SDL_video.h>
#include<glm/gtc/matrix_transform.hpp>

#include"MapLoader.hpp"

#include"render/Material.hpp"
#include"gameobject/GameObjects.hpp"
#include"BaseMeshes.hpp"
#include"FileRW.hpp"
#include"Engine.hpp"
#include"Debug.hpp"

typedef std::shared_ptr<GameObject> GameObjectPtr;
typedef std::shared_ptr<Object_Base3D> Base3DObjectPtr;
typedef std::shared_ptr<Object_Mesh> MeshObjectPtr;
typedef std::shared_ptr<Object_Primitive> PrimitiveObjectPtr;
typedef std::shared_ptr<Object_Light> LightObjectPtr;
typedef std::shared_ptr<Object_DirectionalLight> DirectionalLightPtr;
typedef std::shared_ptr<Object_PointLight> PointLightPtr;
typedef std::shared_ptr<Object_SpotLight> SpotLightPtr;

static Vector3 GetVector3FromJSON(nlohmann::json JSON)
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
		Debug::Log(
			"Could not read Vector3!\nError: '"
			+ std::string(TErr.what())
			+ "'"
		);
	}

	return Vec3;
}

static float GetVersion(std::string MapFileContents)
{
	size_t MatchLocation = MapFileContents.find("#VERSION");

	float Version = 1.f;

	if (MatchLocation != std::string::npos)
	{
		std::string SubStr = MapFileContents.substr(MatchLocation + 9, 6);
		printf("%s\n", SubStr.c_str());
		Version = std::stof(SubStr);
	}
	
	return Version;
}

static void LoadMapVersion1(const char* MapPath, std::string Contents, GameObjectPtr MapParent)
{
	nlohmann::json JSONData;

	try
	{
		JSONData = nlohmann::json::parse(Contents);
	}
	catch (nlohmann::json::exception e)
	{
		const char* whatStr = e.what();

		std::string errStr = std::vformat(
			"Could not parse map data from file '{}': {}",
			std::make_format_args(MapPath, whatStr, Contents)
		);

		throw(errStr);

		return;
	}

	nlohmann::json PartsNode = JSONData["parts"];
	nlohmann::json ModelsNode = JSONData.find("props") != JSONData.end() ? JSONData["props"] : nlohmann::json({});
	nlohmann::json LightsNode = JSONData["lights"];

	std::vector<std::string> LoadedTexturePaths;
	std::vector<uint32_t*> LoadedTextures;

	std::vector<Vertex> ev(0);
	std::vector<uint32_t> ei(0);

	Mesh EmptyMesh = Mesh(ev, ei);

	// need to have this, else everything gets dealloc'd and crashes :)
	// oh, the wonders of pointers
	std::vector<GameObjectPtr>* loadedObjectsReference = new std::vector<GameObjectPtr>();

	for (uint32_t Index = 0; Index < ModelsNode.size(); Index++)
	{
		nlohmann::json PropObject;

		try
		{
			PropObject = ModelsNode[Index];
		}
		catch (nlohmann::json::type_error e)
		{
			const char* whatStr = e.what();
			throw(std::vformat("Failed to decode map data: {}", std::make_format_args(whatStr)));
		}

		std::string ModelPath = PropObject["path"];

		Vector3 Position = GetVector3FromJSON(PropObject["position"]);
		Vector3 Orientation = GetVector3FromJSON(PropObject["orient"]);
		Vector3 Size = GetVector3FromJSON(PropObject["size"]);

		auto& Model = EngineObject::Get()->LoadModelAsMeshesAsync(ModelPath.c_str(), Size, Position);

		if (PropObject.find("name") != PropObject.end())
		{
			if (Model.size() == 0)
				Debug::Log(
					std::vformat(
						"Model was given name '{}' in map file '{}', but has no meshes!",
						std::make_format_args("<cast error>",
							MapPath
						)
					)
				);
			else // TODO: all meshes should be given the name, plus a number probably
				Model[0]->Name = PropObject["name"];
		}

		Mesh* mesh = std::dynamic_pointer_cast<Object_Mesh>(Model[0])->GetRenderMesh();

		auto prop_3d = std::dynamic_pointer_cast<Object_Base3D>(Model[0]);

		if (PropObject.find("facecull") != PropObject.end())
		{
			if ((std::string)PropObject["facecull"] == (std::string)"none")
				prop_3d->FaceCulling = FaceCullingMode::None;

			else if ((std::string)PropObject["facecull"] == (std::string)"front")
				prop_3d->FaceCulling = FaceCullingMode::FrontFace;

			else if ((std::string)PropObject["facecull"] == (std::string)"back")
				prop_3d->FaceCulling = FaceCullingMode::BackFace;

			else
				prop_3d->FaceCulling = FaceCullingMode::BackFace;
		}

		if (PropObject.find("has_transparency") != PropObject.end())
			std::dynamic_pointer_cast<Object_Mesh>(Model[0])->HasTransparency = true;

		prop_3d->ComputePhysics = PropObject.value("computePhysics", 0) == 1 ? true : false;

		if (prop_3d->ComputePhysics)
		{
			Debug::Log(
				std::vformat(
					"'{}' has physics! (computePhysics set to 1 in the .WORLD file)",
					std::make_format_args(prop_3d->Name)
				)
			);
		}
	}

	for (uint32_t Index = 0; Index < PartsNode.size(); Index++)
	{
		nlohmann::json Object = PartsNode[Index];

		int TextureSlot = 0;

		if (Object["type"] == "MeshPart")
		{
			GameObjectPtr NewObject = (GameObjectFactory::CreateGameObject("Mesh"));

			Base3DObjectPtr Object3D = std::dynamic_pointer_cast<Object_Base3D>(NewObject);
			MeshObjectPtr MeshObject = std::dynamic_pointer_cast<Object_Mesh>(NewObject);

			loadedObjectsReference->push_back(NewObject);

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
				RenderMaterial* MeshMaterial = RenderMaterial::GetMaterial(Object["material"]);

				Object3D->Material = MeshMaterial;

			}
			else if (Object.find("textures") != Object.end())
			{

			}

			Object3D->ComputePhysics = Object.value("computePhysics", 0) == 1 ? true : false;

			if (Object3D->ComputePhysics)
			{
				Debug::Log(
					std::vformat(
						"'{}' has physics! (computePhysics set to 1 in the .WORLD file)",
						std::make_format_args(Object3D->Name)
					)
				);
			}
		}

		std::string NewTitle = std::to_string(Index)
								+ "/"
								+ std::to_string(PartsNode.size())
								+ " - "
								+ std::to_string((float)Index / (float)PartsNode.size());
	}

	for (uint32_t LightIndex = 0; LightIndex < LightsNode.size(); LightIndex++)
	{
		nlohmann::json LightObject = LightsNode[LightIndex];

		std::string LightType = LightObject["type"];

		GameObjectPtr Object = (GameObjectFactory::CreateGameObject(LightType));
		LightObjectPtr Light = std::dynamic_pointer_cast<Object_Light>(Object);

		loadedObjectsReference->push_back(Object);

		Light->Position = GetVector3FromJSON(LightObject["position"]);

		Vector3 LightColor = GetVector3FromJSON(LightObject["color"]);

		Light->LightColor = Color(LightColor.X, LightColor.Y, LightColor.Z);

		if (LightType == "PointLight")
		{
			PointLightPtr Pointlight = std::dynamic_pointer_cast<Object_PointLight>(Object);
			Pointlight->Range = LightObject["range"];
		}

		if (LightType == "DirectionalLight")
		{
			DirectionalLightPtr DirectLight = std::dynamic_pointer_cast<Object_DirectionalLight>(Object);
			DirectLight->Position = GetVector3FromJSON(LightObject["position"]);
			DirectLight->LightColor = Color(LightColor.X, LightColor.Y, LightColor.Z);
		}

		Object->SetParent(MapParent);
	}
}

static void LoadMapVersion2(const char* MapPath, std::string Contents, GameObjectPtr MapParent)
{
	nlohmann::json JSONData;

	try
	{
		JSONData = nlohmann::json::parse(Contents);
	}
	catch (nlohmann::json::exception e)
	{
		const char* whatStr = e.what();

		std::string ErrStr = std::vformat(
			"V2: Could not parse map data from file '{}': {}",
			std::make_format_args(MapPath, whatStr, Contents)
		);

		throw(ErrStr);

		return;
	}

	nlohmann::json GameObjectsNode = JSONData["GameObjects"];

	// need to have this, else everything gets dealloc'd and crashes :)
	// oh, the wonders of pointers
	std::vector<GameObjectPtr>* loadedObjectsReference = new std::vector<GameObjectPtr>();

	for (nlohmann::json Item : GameObjectsNode)
	{
		std::string Class = Item["ClassName"];
		std::string Name = Item["Name"];

		GameObjectPtr NewObject = GameObjectFactory::CreateGameObject(Class);

		loadedObjectsReference->push_back(NewObject);

		// https://json.nlohmann.me/features/iterators/#access-object-key-during-iteration
		for (auto it = Item.begin(); it != Item.end(); ++it)
		{
			std::string MemberName = it.key();

			if (MemberName == "ClassName")
				continue;

			auto MemberIter = NewObject->GetProperties().find(MemberName);

			if (MemberIter == NewObject->GetProperties().end())
			{
				const char* FmtStr = "Deserialize warning: Member '{}' is not defined in the API for the Class {} (Name: '{}')!";
				auto FmtArgs = std::make_format_args(
					MemberName,
					Class,
					Name
				);
				Debug::Log(std::vformat(FmtStr, FmtArgs));

				continue;
			}

			auto& Member = NewObject->GetProperty(MemberName);

			Reflection::ValueType MemberType = Member.Type;
			int MemberTypeInt = int(MemberType);

			auto& PropSetter = Member.Setter;

			if (!PropSetter)
			{
				const char* FmtStr = "Deserialize error: Member '{}' of {} '{}' has no prop setter!";
				auto FmtArgs = std::make_format_args(
					MemberName,
					Class,
					Name
				);
				Debug::Log(std::vformat(FmtStr, FmtArgs));

				continue;
			}

			switch (MemberType)
			{

			case (Reflection::ValueType::String):
			{
				std::string PropValue = Item[MemberName];
				PropSetter(PropValue);
				break;
			}

			case (Reflection::ValueType::Bool):
			{
				bool PropValue = Item[MemberName];
				PropSetter(PropValue);
				break;
			}

			case (Reflection::ValueType::Double):
			{
				double PropValue = Item[MemberName];
				PropSetter(PropValue);
				break;
			}

			case (Reflection::ValueType::Integer):
			{
				int PropValue = Item[MemberName];
				PropSetter(PropValue);
				break;
			}

			case (Reflection::ValueType::Color):
			{
				Vector3 PropVec3 = GetVector3FromJSON(Item[MemberName]);
				Color PropValue = Color(PropVec3.X, PropVec3.Y, PropVec3.Z);
				PropSetter(PropValue);
				break;
			}

			case (Reflection::ValueType::Vector3):
			{
				Vector3 PropValue = GetVector3FromJSON(Item[MemberName]);
				PropSetter(PropValue);
				break;
			}

			default:
			{
				const char* FmtStr = "Deserialize warning: Not reading prop '{}' of class {} because it's type ({}) is unknown";
				auto FmtArgs = std::make_format_args(
					MemberName,
					Class,
					MemberTypeInt
				);
				Debug::Log(std::vformat(FmtStr, FmtArgs));
				break;
			}

			}
		}

		NewObject->SetParent(MapParent);
	}
}

void MapLoader::LoadMapIntoObject(const char* MapFilePath, GameObjectPtr MapParent)
{
	bool MapExists = true;
	std::string FileContents = FileRW::ReadFile(MapFilePath, &MapExists);

	//TODO fix de-allocation issues, std::shared_ptr has caused the majority of runtime access
	// violations ever for the engine
	// pwf 2022
	// idk, TODO find better solution future me

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

	float Version = GetVersion(FileContents);
	Debug::Log(std::vformat("The Map Version is {}", std::make_format_args(Version)));

	size_t JsonStartLoc = FileContents.find("{");

	std::string JsonFileContents = FileContents.substr(JsonStartLoc);

	//try
	//{
		if (Version == 1.f)
			LoadMapVersion1(MapFilePath, JsonFileContents, MapParent);
		else
			if (Version == 2.f)
				LoadMapVersion2(MapFilePath, JsonFileContents, MapParent);
	//}
	//catch (nlohmann::json::type_error e)
	/*{
		std::string errmsg = (std::string(e.what()) + " with contents:\n" + JsonFileContents);

		Debug::Log(errmsg);

		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR,
			"map load error (NLJSON)",
			errmsg.c_str(),
			SDL_GetGrabbedWindow()
		);
	}*/

	Debug::Log(
		std::vformat(
			"Loaded map file '{}'",
			std::make_format_args(
				MapFilePath
			)
		)
	);
}
