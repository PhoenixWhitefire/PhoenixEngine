// TODO: cleanup code!

#include<nljson.hpp>
#include<SDL2/SDL_messagebox.h>
#include<SDL2/SDL_video.h>
#include<glm/gtc/matrix_transform.hpp>

#include"SceneFormat.hpp"

#include"gameobject/GameObjects.hpp"
#include"render/Material.hpp"
#include"ModelLoader.hpp"
#include"BaseMeshes.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

static std::string errorString = "No error";

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

		mesh->Transform = glm::translate(mesh->Transform, (glm::vec3)Position);
		mesh->Size = mesh->Size * Size;
	}

	return Loader.LoadedObjs;
}

static Vector3 GetVector3FromJson(const nlohmann::json& Json)
{
	int Index = -1;

	Vector3 Vec3;

	try
	{
		Vec3 = Vector3(
			Json[++Index],
			Json[++Index],
			Json[++Index]
		);
	}
	catch (nlohmann::json::type_error TErr)
	{
		Debug::Log(
			"Could not read Vector3: '"
			+ std::string(TErr.what())
			+ "'"
		);
	}

	return Vec3;
}

static Color GetColorFromJson(const nlohmann::json& Json)
{
	int Index = -1;

	Color col;

	try
	{
		col = Color(
			Json[++Index],
			Json[++Index],
			Json[++Index]
		);
	}
	catch (nlohmann::json::type_error TErr)
	{
		Debug::Log(
			"Could not read Color: '"
			+ std::string(TErr.what())
			+ "'"
		);
	}

	return col;
}

static glm::mat4 GetMatrixFromJson(const nlohmann::json& Json)
{
	glm::mat4 mat{};

	try
	{
		// 01/09/2024:
		// not even explicitly saying they're `float`s with
		// "<4, 4, float>" will get GLM to cast them :pensive:
		// like bro they'll listen to you if you tell them
		// don't be angry at them they don't even know what you want them to do
		// :,(

		for (int col = 0; col < 4; col++)
			for (int row = 0; row < 4; row++)
				mat[col][row] = float(Json[col][row]);
	}
	catch (nlohmann::json::type_error TErr)
	{
		Debug::Log(
			"Could not read Matrix: '"
			+ std::string(TErr.what())
			+ "'"
		);
	}

	return mat;
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

static std::vector<GameObject*> LoadMapVersion1(
	const std::string& Contents,
	bool* SuccessPtr
)
{
	nlohmann::json JsonData;

	try
	{
		JsonData = nlohmann::json::parse(Contents);
	}
	catch (nlohmann::json::exception e)
	{
		const char* whatStr = e.what();

		errorString = std::vformat(
			"JSON Parsing error: {}",
			std::make_format_args(whatStr)
		);

		*SuccessPtr = false;

		return {};
	}

	std::string sceneName = JsonData.value("SceneName", "<UNNAMED SCENE>");

	nlohmann::json PartsNode = JsonData["parts"];
	nlohmann::json ModelsNode = JsonData.value("props", nlohmann::json{});
	nlohmann::json LightsNode = JsonData["lights"];

	std::vector<GameObject*> Objects;

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

		Vector3 Position = GetVector3FromJson(PropObject["position"]);
		Vector3 Orientation = GetVector3FromJson(PropObject["orient"]);
		Vector3 Size = GetVector3FromJson(PropObject["size"]);

		std::vector<GameObject*> Model = LoadModelAsMeshes(ModelPath.c_str(), Size, Position);

		std::string modelName = PropObject.value("name", "<UN-NAMED>");

		if (Model.size() == 0)
			Debug::Log(
				std::vformat(
					"Model '{}' in map file '{}' has no meshes!",
					std::make_format_args(
						modelName,
						sceneName
					)
				)
			);
		else
		{
			for (uint32_t index = 0; index < Model.size(); index++)
			{
				GameObject* mesh = Model[index];
				mesh->Name = std::vformat("{}_{}", std::make_format_args(modelName, index));
				Objects.push_back(mesh);
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

		if (Object["type"] == "MeshPart")
		{
			// Yeah, "MeshPart" as the type and `Primitive`s as the result.
			// Like just use V2 bro why the hell you botherin' this musty-ahh
			// format?
			GameObject* NewObject = GameObject::CreateGameObject("Primitive");
			Objects.push_back(NewObject);

			Object_Base3D* Object3D = dynamic_cast<Object_Base3D*>(NewObject);

			NewObject->Name = Object.value("Name", NewObject->Name);

			Vector3 Position = GetVector3FromJson(Object["position"]);
			Vector3 Orientation;

			if (Object.find("orient") != Object.end())
			{
				Orientation = GetVector3FromJson(Object["orient"]);
				/*
				Orientation = Vector3(
					Orientation.Y,
					Orientation.Z,
					Orientation.X
				);
				*/
			}

			Object3D->Transform = glm::translate(Object3D->Transform, (glm::vec3)Position);

			Object3D->Transform = glm::rotate(
				Object3D->Transform,
				glm::radians(static_cast<float>(Orientation.X)),
				glm::vec3(1.0f, 0.0f, 0.0f)
			);
			Object3D->Transform = glm::rotate(
				Object3D->Transform,
				glm::radians(static_cast<float>(Orientation.Y)),
				glm::vec3(0.0f, 1.0f, 0.0f)
			);
			Object3D->Transform = glm::rotate(
				Object3D->Transform,
				glm::radians(static_cast<float>(Orientation.Z)),
				glm::vec3(0.0f, 0.0f, 1.0f)
			);

			if (Object.find("color") != Object.end())
				Object3D->ColorRGB = GetColorFromJson(Object["color"]);

			Object3D->Size = GetVector3FromJson(Object["size"]);

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
		else
			throw("Item " + std::to_string(Index) + " of `parts` node doesn't have a `type = 'MeshPart'` key");

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
		Objects.push_back(Object);

		Object_Light* Light = dynamic_cast<Object_Light*>(Object);

		Light->Position = GetVector3FromJson(LightObject["position"]);

		Light->LightColor = GetColorFromJson(LightObject["color"]);

		if (LightType == "PointLight")
		{
			Object_PointLight* Pointlight = dynamic_cast<Object_PointLight*>(Object);
			Pointlight->Range = LightObject["range"];
		}
	}

	return Objects;
}

static std::vector<GameObject*> LoadMapVersion2(const std::string& Contents, bool* Success)
{
	nlohmann::json JsonData;

	try
	{
		JsonData = nlohmann::json::parse(Contents);
	}
	catch (nlohmann::json::exception e)
	{
		const char* whatStr = e.what();

		errorString = std::vformat(
			"V2 - JSON Parsing error: {}",
			std::make_format_args(whatStr)
		);

		*Success = false;

		return {};
	}

	std::string sceneName = JsonData.value("SceneName", "<UNNAMED SCENE>");

	if (JsonData.find("GameObjects") == JsonData.end())
	{
		errorString = std::vformat(
			"The `GameObjects` key is not present in scene '{}'",
			std::make_format_args(sceneName)
		);

		*Success = false;

		return {};
	}

	nlohmann::json GameObjectsNode = JsonData["GameObjects"];

	std::unordered_map<int64_t, GameObject*> objectsMap;
	std::unordered_map<int64_t, int64_t> realIdToSceneId;
	std::unordered_map<GameObject*, std::unordered_map<std::string, uint32_t>> objectProps;

	for (uint32_t itemIndex = 0; itemIndex < GameObjectsNode.size(); itemIndex++)
	{
		nlohmann::json item = GameObjectsNode[itemIndex];

		if (item.find("ClassName") == item.end())
		{
			*Success = false;
			errorString = std::vformat(
				"Object #{} is missing it's `ClassName` key",
				std::make_format_args(itemIndex)
			);

			continue;
		}

		std::string Class = item["ClassName"];

		if (item.find("Name") == item.end())
		{
			*Success = false;
			errorString = std::vformat(
				"Object #{} (a {}) was missing it's `Name` key.",
				std::make_format_args(itemIndex, Class)
			);

			continue;
		}

		std::string Name = item["Name"];

		GameObject* NewObject = GameObject::CreateGameObject(Class);

		objectsMap.insert(std::pair(item.value("ObjectId", PHX_GAMEOBJECT_NULL_ID), NewObject));
		realIdToSceneId.insert(std::pair(NewObject->ObjectId, item.value("ObjectId", PHX_GAMEOBJECT_NULL_ID)));

		objectProps.insert(std::pair(NewObject, std::unordered_map<std::string, uint32_t>{}));

		// https://json.nlohmann.me/features/iterators/#access-object-key-during-iteration
		for (auto memberIt = item.begin(); memberIt != item.end(); ++memberIt)
		{
			std::string MemberName = memberIt.key();

			if (MemberName == "ClassName" || MemberName == "ObjectId")
				continue;
			
			nlohmann::json MemberValue = memberIt.value();

			bool HasProp = NewObject->HasProperty(MemberName);

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

			auto& Member = NewObject->GetProperty(MemberName);

			Reflection::ValueType MemberType = Member.Type;
			
			auto& PropSetter = Member.Set;

			if (!PropSetter)
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
				std::string PropValue = MemberValue;
				auto gv = Reflection::GenericValue(PropValue);
				PropSetter(NewObject, gv);
				break;
			}

			case (Reflection::ValueType::Bool):
			{
				bool PropValue = MemberValue;
				auto gv = Reflection::GenericValue(PropValue);
				PropSetter(NewObject, gv);
				break;
			}

			case (Reflection::ValueType::Double):
			{
				double PropValue = MemberValue;
				auto gv = Reflection::GenericValue(PropValue);
				PropSetter(NewObject, gv);
				break;
			}

			case (Reflection::ValueType::Integer):
			{
				int PropValue = MemberValue;
				auto gv = Reflection::GenericValue(PropValue);
				PropSetter(NewObject, gv);
				break;
			}

			case (Reflection::ValueType::Color):
			{
				Vector3 PropVec3 = GetVector3FromJson(MemberValue);
				Color PropValue = Color(
					static_cast<float>(PropVec3.X),
					static_cast<float>(PropVec3.Y),
					static_cast<float>(PropVec3.Z)
				);
				Reflection::GenericValue gv = PropValue.ToGenericValue();
				PropSetter(NewObject, gv);
				break;
			}

			case (Reflection::ValueType::Vector3):
			{
				Vector3 PropValue = GetVector3FromJson(MemberValue);
				Reflection::GenericValue gv = PropValue.ToGenericValue();
				PropSetter(NewObject, gv);
				break;
			}

			case (Reflection::ValueType::Matrix):
			{
				glm::mat4 PropValue = GetMatrixFromJson(MemberValue);
				Reflection::GenericValue gv(PropValue);
				PropSetter(NewObject, gv);
				break;
			}

			case (Reflection::ValueType::GameObject):
			{
				objectProps[NewObject].insert(std::pair(MemberName, MemberValue));
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
	}

	std::vector<GameObject*> Objects;

	for (auto& it : objectsMap)
	{
		GameObject* object = it.second;

		// !! IMPORTANT !!
		// The `Parent` key *should not* be set for Root Nodes as their parent
		// *is not part of the scene!*
		// 04/09/2024
		if (objectProps[object].find("Parent") == objectProps[object].end())
			Objects.push_back(object);

		for (auto& objectProp : objectProps[object])
		{
			const std::string& propName = objectProp.first;
			const uint32_t sceneRelativeId = objectProp.second;

			auto target = objectsMap.find(sceneRelativeId);

			if (target != objectsMap.end())
			{
				Reflection::GenericValue gv;
				gv.Type = Reflection::ValueType::GameObject;
				gv.Integer = target->second->ObjectId;

				object->SetPropertyValue(propName, gv);
			}
			else
			{
				Debug::Log(std::vformat(
					"{} '{}' refers to invalid scene-relative Object ID {} for prop {}. To avoid UB, it will be NULL'd.",
					std::make_format_args(
						object->ClassName,
						object->Name,
						sceneRelativeId,
						propName
					)
				));

				Reflection::GenericValue gv;
				gv.Type = Reflection::ValueType::GameObject;
				gv.Integer = 0;

				object->SetPropertyValue(propName, gv);
			}
		}
	}

	return Objects;
}

std::vector<GameObject*> SceneFormat::Deserialize(
	const std::string& Contents,
	bool* SuccessPtr
)
{
	if (Contents == "")
	{
		errorString = std::vformat("Empty scene file: '{}'", std::make_format_args(Contents));
		*SuccessPtr = false;

		return {};
	}

	float version = GetVersion(Contents);
	Debug::Log(std::vformat("Scene version is {}", std::make_format_args(version)));

	size_t jsonStartLoc = Contents.find("{");
	std::string jsonFileContents = Contents.substr(jsonStartLoc);

	std::vector<GameObject*> objects{};

	if (version == 1.f)
		objects = LoadMapVersion1(jsonFileContents, SuccessPtr);
	else
		if (version == 2.f)
			objects = LoadMapVersion2(jsonFileContents, SuccessPtr);

	Debug::Log("Scene loaded");

	return objects;
}

static nlohmann::json serializeObject(GameObject* Object, bool IsRootNode = false)
{
	nlohmann::json item{};

	for (auto& prop : Object->GetProperties())
	{
		const std::string& propName = prop.first;
		const IProperty& propInfo = prop.second;

		if (!propInfo.Set && propName != "ObjectId" && propName != "ClassName")
			continue;

		// !! IMPORTANT !!
		// The `Parent` key *should not* be set for Root Nodes as their parent
		// *is not part of the scene!*
		// 04/09/2024
		if (IsRootNode && propName == "Parent")
			continue;

		Reflection::GenericValue value = propInfo.Get(Object);

		switch (prop.second.Type)
		{
		case (Reflection::ValueType::Bool):
		{
			item[propName] = value.AsBool();
			break;
		}
		case (Reflection::ValueType::Integer):
		{
			item[propName] = value.AsInt();
			break;
		}
		case (Reflection::ValueType::Double):
		{
			item[propName] = value.AsDouble();
			break;
		}
		case (Reflection::ValueType::String):
		{
			item[propName] = value.AsString();
			break;
		}
		
		case (Reflection::ValueType::Color):
		{
			Color col(value);
			item[propName] = { col.R, col.G, col.B };
			break;
		}
		case (Reflection::ValueType::Vector3):
		{
			Vector3 vec(value);
			item[propName] = { vec.X, vec.Y, vec.Z };
			break;
		}
		case (Reflection::ValueType::Matrix):
		{
			glm::mat4 mat = value.AsMatrix();
			item[propName] = nlohmann::json::array();

			for (int col = 0; col < 4; col++)
			{
				item[propName][col] = nlohmann::json::array();

				for (int row = 0; row < 4; row++)
					item[propName][col][row] = mat[col][row];
			}
				
			break;
		}

		case (Reflection::ValueType::GameObject):
		{
			auto target = GameObject::s_WorldArray.find(static_cast<uint32_t>(value.Integer));

			if (target != GameObject::s_WorldArray.end())
				item[propName] = value.Integer;
			else
				item[propName] = PHX_GAMEOBJECT_NULL_ID;
		}
		}
	}

	return item;
}

std::string SceneFormat::Serialize(std::vector<GameObject*> Objects, const std::string& SceneName)
{
	nlohmann::json json;

	json["SceneName"] = SceneName;

	nlohmann::json objectsArray = nlohmann::json::array();

	for (GameObject* rootObject : Objects)
	{
		objectsArray.push_back(serializeObject(rootObject, true));
		
		for (GameObject* desc : rootObject->GetDescendants())
			objectsArray.push_back(serializeObject(desc));
	}

	json["GameObjects"] = objectsArray;

	std::string contents = "// Automatically generated as " + SceneName
							+ "\n#Version 02.00\n\n"
							+ json.dump(2);

	return contents;
}

std::string SceneFormat::GetLastErrorString()
{
	return errorString;
}
