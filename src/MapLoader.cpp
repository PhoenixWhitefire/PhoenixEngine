// TODO: cleanup code!

#include<nljson.h>
#include<SDL2/SDL_messagebox.h>
#include<SDL2/SDL_video.h>
#include<glm/gtc/matrix_transform.hpp>

#include"MapLoader.hpp"

#include"gameobject/GameObjects.hpp"
#include"render/Material.hpp"
#include"ModelLoader.hpp"
#include"BaseMeshes.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

static auto LoadModelAsMeshes(
	const char* ModelFilePath,
	Vector3 Size,
	Vector3 Position,
	bool AutoParent = true
)
{
	ModelLoader Loader(ModelFilePath, AutoParent ? dynamic_cast<GameObject*>(GameObject::s_DataModel) : nullptr);

	for (GameObject* object : Loader.LoadedObjs)
	{
		Object_Mesh* mesh = dynamic_cast<Object_Mesh*>(object);

		mesh->Matrix = glm::translate(mesh->Matrix, (glm::vec3)Position);
		mesh->Size = mesh->Size * Size;
	}

	return Loader.LoadedObjs;
}

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

static float GetVersion(std::string const& MapFileContents)
{
	size_t MatchLocation = MapFileContents.find("#Version");

	float Version = 1.f;

	if (MatchLocation != std::string::npos)
	{
		std::string SubStr = MapFileContents.substr(MatchLocation + 9, 4);
		Version = std::stof(SubStr);
	}
	
	return Version;
}

static void LoadMapVersion1(const std::string& MapPath, const std::string& Contents, GameObject* MapParent)
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

	if (JSONData.find("parts") == JSONData.end())
		throw(std::vformat(
			"Required node `parts` in map file '{}' not present",
			std::make_format_args(MapPath)
		));

	if (JSONData.find("lights") == JSONData.end())
		throw(std::vformat(
			"Required node `lights` in map file '{}' not present",
			std::make_format_args(MapPath)
		));

	nlohmann::json PartsNode = JSONData["parts"];
	nlohmann::json ModelsNode = JSONData.value("props", nlohmann::json{});
	nlohmann::json LightsNode = JSONData["lights"];

	std::vector<std::string> LoadedTexturePaths;
	std::vector<uint32_t*> LoadedTextures;

	std::vector<Vertex> ev(0);
	std::vector<uint32_t> ei(0);

	Mesh EmptyMesh = Mesh(ev, ei);

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

		std::vector<GameObject*> Model = LoadModelAsMeshes(ModelPath.c_str(), Size, Position);

		std::string modelName = PropObject.value("name", "<UN-NAMED>");

		if (Model.size() == 0)
			Debug::Log(
				std::vformat(
					"Model '{}' in map file '{}' has no meshes!",
					std::make_format_args(
						modelName,
						MapPath
					)
				)
			);
		else
		{
			for (uint32_t index = 0; index < Model.size(); index++)
			{
				GameObject* mesh = Model[index];
				mesh->Name = std::vformat("{}_{}", std::make_format_args(modelName, index));
				mesh->SetParent(GameObject::s_DataModel->GetChild("Workspace")->GetChild("Level"));
			}
		}

		auto prop_3d = dynamic_cast<Object_Base3D*>(Model[0]);

		if (PropObject.find("facecull") != PropObject.end())
		{
			std::string facecullNameStr = std::string(PropObject["facecull"]);
			const char* facecullName = facecullNameStr.c_str();

			if (strcmp(facecullName, "none") == 0)
				prop_3d->FaceCulling = FaceCullingMode::None;

			else if (strcmp(facecullName, "front") == 0)
				prop_3d->FaceCulling = FaceCullingMode::FrontFace;

			else if (strcmp(facecullName, "back") == 0)
				prop_3d->FaceCulling = FaceCullingMode::BackFace;

			else
				prop_3d->FaceCulling = FaceCullingMode::BackFace;
		}

		if (PropObject.find("has_transparency") != PropObject.end())
			dynamic_cast<Object_Mesh*>(Model[0])->HasTransparency = true;

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
			// Yeah, "MeshPart" as the type and `Primitive`s as the result.
			// Like just use V2 bro why the hell you botherin' this musty-ahh
			// format?
			GameObject* NewObject = GameObject::CreateGameObject("Primitive");

			Object_Base3D* Object3D = dynamic_cast<Object_Base3D*>(NewObject);

			NewObject->Name = Object.value("Name", NewObject->Name);

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

			Object3D->Size = GetVector3FromJSON(Object["size"]);

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

		GameObject* Object = (GameObject::CreateGameObject(LightType));
		Object_Light* Light = dynamic_cast<Object_Light*>(Object);

		Light->Position = GetVector3FromJSON(LightObject["position"]);

		Vector3 LightColor = GetVector3FromJSON(LightObject["color"]);

		Light->LightColor = Color(LightColor.X, LightColor.Y, LightColor.Z);

		if (LightType == "PointLight")
		{
			Object_PointLight* Pointlight = dynamic_cast<Object_PointLight*>(Object);
			Pointlight->Range = LightObject["range"];
		}

		if (LightType == "DirectionalLight")
		{
			Object_DirectionalLight* DirectLight = dynamic_cast<Object_DirectionalLight*>(Object);
			DirectLight->Position = GetVector3FromJSON(LightObject["position"]);
			DirectLight->LightColor = Color(LightColor.X, LightColor.Y, LightColor.Z);
		}

		Object->SetParent(MapParent);
	}
}

static void LoadMapVersion2(const std::string& MapPath, const std::string& Contents, GameObject* MapParent)
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

	if (JSONData.find("GameObjects") == JSONData.end())
		throw(std::vformat(
			"The `GameObjects` key is not present in the map file '{}'",
			std::make_format_args(MapPath)
		));

	nlohmann::json GameObjectsNode = JSONData["GameObjects"];

	for (nlohmann::json Item : GameObjectsNode)
	{
		if (Item.find("ClassName") == Item.end())
			throw("An object was missing it's `ClassName` key.");

		std::string Class = Item["ClassName"];

		if (Item.find("Name") == Item.end())
			throw(std::vformat(
				"A {} was missing it's `Name` key.",
				std::make_format_args(Class)
			));

		std::string Name = Item["Name"];

		GameObject* NewObject = GameObject::CreateGameObject(Class);

		// https://json.nlohmann.me/features/iterators/#access-object-key-during-iteration
		for (auto it = Item.begin(); it != Item.end(); ++it)
		{
			std::string MemberName = it.key();

			if (MemberName == "ClassName")
				continue;
			
			printf(
				"%s CLASS HAS %zi PROPERTIES (%zi CASTED), CUR: %s\n",
				Class.c_str(),
				NewObject->ApiReflection->GetProperties().size(),
				dynamic_cast<Object_Mesh*>(NewObject)->ApiReflection->GetProperties().size(),
				MemberName.c_str()
			);

			for (auto& p : NewObject->ApiReflection->GetProperties())
				printf("%s\n", p.first.c_str());

			bool HasProp = NewObject->ApiReflection->HasProperty(MemberName);

			if (!HasProp)
			{
				const char* FmtStr = "Deserialization warning: Member '{}' is not defined in the API for the Class {} (Name: '{}')!";
				auto FmtArgs = std::make_format_args(
					MemberName,
					Class,
					Name
				);
				Debug::Log(std::vformat(FmtStr, FmtArgs));

				continue;
			}

			auto& Member = NewObject->ApiReflection->GetProperty(MemberName);

			Reflection::ValueType MemberType = Member.Type;
			
			auto& PropSetter = Member.Set;

			if (!Member.Settable)
			{
				const char* FmtStr = "Deserialization warning: Member '{}' of {} '{}' is read-only!";
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
				PropSetter(NewObject, PropValue);
				break;
			}

			case (Reflection::ValueType::Bool):
			{
				bool PropValue = Item[MemberName];
				PropSetter(NewObject, PropValue);
				break;
			}

			case (Reflection::ValueType::Double):
			{
				double PropValue = Item[MemberName];
				PropSetter(NewObject, PropValue);
				break;
			}

			case (Reflection::ValueType::Integer):
			{
				int PropValue = Item[MemberName];
				PropSetter(NewObject, PropValue);
				break;
			}

			case (Reflection::ValueType::Color):
			{
				Vector3 PropVec3 = GetVector3FromJSON(Item[MemberName]);
				Color PropValue = Color(PropVec3.X, PropVec3.Y, PropVec3.Z);
				Reflection::GenericValue gv = PropValue.ToGenericValue();
				PropSetter(NewObject, gv);
				break;
			}

			case (Reflection::ValueType::Vector3):
			{
				Vector3 PropValue = GetVector3FromJSON(Item[MemberName]);
				Reflection::GenericValue gv = PropValue.ToGenericValue();
				PropSetter(NewObject, gv);
				break;
			}

			default:
			{
				const char* FmtStr = "Deserialization warning: Not reading prop '{}' of class {} because it's type ({}) is unknown";
				std::string MemberTypeName = Reflection::TypeAsString(MemberType);
				auto FmtArgs = std::make_format_args(
					MemberName,
					Class,
					MemberTypeName
				);
				Debug::Log(std::vformat(FmtStr, FmtArgs));
				break;
			}

			}
		}

		NewObject->SetParent(MapParent);
	}
}

void MapLoader::LoadMapIntoObject(const std::string& MapFilePath, GameObject* MapParent)
{
	bool MapExists = true;
	std::string FileContents = FileRW::ReadFile(MapFilePath, &MapExists);

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

	if (Version == 1.f)
		LoadMapVersion1(MapFilePath, JsonFileContents, MapParent);
	else
		if (Version == 2.f)
			LoadMapVersion2(MapFilePath, JsonFileContents, MapParent);
		
	Debug::Log(
		std::vformat(
			"Loaded map file '{}'",
			std::make_format_args(
				MapFilePath
			)
		)
	);
}
