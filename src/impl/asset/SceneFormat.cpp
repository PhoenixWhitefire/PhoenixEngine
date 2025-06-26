// TODO: cleanup code!

#include <chrono>
#include <nljson.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "asset/SceneFormat.hpp"

#include "asset/PrimitiveMeshes.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/ModelImporter.hpp"
#include "component/Transform.hpp"
#include "component/Light.hpp"
#include "component/Mesh.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

#define SF_EMIT_WARNING(err, ...) Log::Warning(std::format(     \
	"Deserialization warning: " err,                            \
	__VA_ARGS__                                                 \
))                                                              \

static std::string errorString = "No error";

static auto LoadModelAsMeshes(
	const char* ModelFilePath,
	glm::vec3 Size,
	glm::vec3 Position,
	bool AutoParent = true
)
{
	ModelLoader Loader(ModelFilePath, AutoParent ? GameObject::GetObjectById(GameObject::s_DataModel) : nullptr);

	for (GameObject* object : Loader.LoadedObjs)
	{
		EcTransform* mesh = object->GetComponent<EcTransform>();

		if (mesh)
		{
			mesh->Transform = glm::translate(mesh->Transform, (glm::vec3)Position);
			mesh->Size = mesh->Size * Size;
		}
	}

	return Loader.LoadedObjs;
}

static glm::vec3 GetVector3FromJson(const nlohmann::json& Json)
{
	glm::vec3 Vec3;

	try
	{
		Vec3 = glm::vec3(
			Json[0],
			Json[1],
			Json[2]
		);
	}
	catch (const nlohmann::json::type_error& TErr)
	{
		Log::Warning(
			"Could not read Vector3: '"
			+ std::string(TErr.what())
			+ "'"
		);
	}

	return Vec3;
}

static Color GetColorFromJson(const nlohmann::json& Json)
{
	Color col;

	try
	{
		col = Color(
			Json[0],
			Json[1],
			Json[2]
		);
	}
	catch (const nlohmann::json::type_error& TErr)
	{
		Log::Warning(
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
	catch (const nlohmann::json::type_error& TErr)
	{
		Log::Warning(
			"Could not read Matrix: '"
			+ std::string(TErr.what())
			+ "'"
		);
	}

	return mat;
}

static float getVersion(const std::string& MapFileContents)
{
	size_t matchLocation = MapFileContents.find("#Version");

	float version = 1.f;

	if (matchLocation != std::string::npos)
	{
		std::string subStr = MapFileContents.substr(matchLocation + 9, 4);
		version = std::stof(subStr);
	}
	
	return version;
}

static std::vector<GameObjectRef> LoadMapVersion1(
	const std::string& Contents,
	bool* SuccessPtr
)
{
	ZoneScoped;

	nlohmann::json JsonData;

	try
	{
		ZoneScopedN("ParseJson");
		JsonData = nlohmann::json::parse(Contents);
	}
	catch (const nlohmann::json::parse_error& e)
	{
		errorString = std::format(
			"JSON Parsing error: {}",
			e.what()
		);

		*SuccessPtr = false;

		return {};
	}

	std::string sceneName = JsonData.value("SceneName", "<UNNAMED SCENE>");

	nlohmann::json PartsNode = JsonData["parts"];
	nlohmann::json ModelsNode = JsonData.value("props", nlohmann::json{});
	nlohmann::json LightsNode = JsonData["lights"];

	std::vector<GameObjectRef> Objects;
	Objects.reserve(PartsNode.size() + ModelsNode.size() + LightsNode.size());

	for (uint32_t Index = 0; Index < ModelsNode.size(); Index++)
	{
		nlohmann::json PropObject;

		try
		{
			PropObject = ModelsNode[Index];
		}
		catch (const nlohmann::json::type_error& e)
		{
			throw(std::format("Failed to decode map data: {}", e.what()));
		}

		std::string ModelPath = PropObject["path"];

		glm::vec3 Position = GetVector3FromJson(PropObject["position"]);
		//Vector3 Orientation = GetVector3FromJson(PropObject["orient"]);
		glm::vec3 Size = GetVector3FromJson(PropObject["size"]);

		std::vector<GameObject*> Model = LoadModelAsMeshes(ModelPath.c_str(), Size, Position);

		std::string modelName = PropObject.value("name", "<UN-NAMED>");

		if (Model.empty())
			Log::Warning(
				std::format(
					"Model '{}' in map file '{}' has no meshes!",
					modelName,
					sceneName
				)
			);
		else
		{
			if (Model.size() > 1)
			{
				GameObject* container = GameObject::Create("Model");

				for (size_t index = 0; index < Model.size(); index++)
				{
					GameObject* mesh = Model[index];
					mesh->SetParent(container);
				}

				container->Name = modelName;

				Objects.push_back(container);
			}
			else
			{
				Model[0]->Name = modelName;
				Objects.push_back(Model[0]);
			}
		}

		for (GameObject* obj : Model)
		{
			auto prop_3d = obj->GetComponent<EcMesh>();

			if (!prop_3d)
				continue;

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

			prop_3d->PhysicsDynamics = PropObject.value("computePhysics", 0) == 1 ? true : false;
		}
	}

	MaterialManager* mtlManager = MaterialManager::Get();

	for (uint32_t Index = 0; Index < PartsNode.size(); Index++)
	{
		nlohmann::json Object = PartsNode[Index];

		GameObject* NewObject = GameObject::Create("Primitive");
		Objects.push_back(NewObject);

		EcTransform* ct = NewObject->GetComponent<EcTransform>();
		EcMesh* cm = NewObject->GetComponent<EcMesh>();

		NewObject->Name = Object.value("name", NewObject->Name);

		glm::vec3 Position = GetVector3FromJson(Object["position"]);
		glm::vec3 Orientation;

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

		ct->Transform = glm::translate(ct->Transform, (glm::vec3)Position);

		// YXZ Rotation order
		ct->Transform = glm::rotate(
			ct->Transform,
			glm::radians(Orientation.y),
			glm::vec3(0.f, 1.f, 0.f)
		);
		ct->Transform = glm::rotate(
			ct->Transform,
			glm::radians(Orientation.x),
			glm::vec3(1.f, 0.f, 0.f)
		);
		ct->Transform = glm::rotate(
			ct->Transform,
			glm::radians(Orientation.z),
			glm::vec3(0.f, 0.f, 1.f)
		);

		if (Object.find("color") != Object.end())
			cm->Tint = GetColorFromJson(Object["color"]);

		ct->Size = GetVector3FromJson(Object["size"]);

		if (Object.find("material") != Object.end())
		{
			uint32_t MeshMaterial = mtlManager->LoadFromPath(std::string(Object["material"]));

			cm->MaterialId = MeshMaterial;

		}
		else if (Object.find("textures") != Object.end())
		{

		}

		cm->PhysicsDynamics = Object.value("computePhysics", 0) == 1 ? true : false;

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

		GameObject* Object = GameObject::Create(LightType);
		Objects.push_back(Object);

		EcTransform* ct = Object->GetComponent<EcTransform>();

		ct->Transform = glm::translate(
			glm::mat4(1.f),
			(glm::vec3)GetVector3FromJson(LightObject["position"])
		);

		//Light->LightColor = GetColorFromJson(LightObject["color"]);

		if (LightType == "PointLight")
		{
			EcPointLight* Pointlight = Object->GetComponent<EcPointLight>();
			Pointlight->Range = LightObject["range"];
		}
	}

	return Objects;
}

static GameObject* createObjectFromJsonItem(const nlohmann::json& Item, uint32_t ItemIndex, float Version)
{
	if (Version == 2.f)
	{
		const auto& classNameJson = Item.find("$_class");

		if (classNameJson == Item.end())
		{
			SF_EMIT_WARNING("Object #{} was missing it's '$_class' key, skipping", ItemIndex);

			return nullptr;
		}

		std::string className = classNameJson.value();

		if (className == "Primitive")
			className = "Mesh";

		return GameObject::Create(className);
	}
	else
	{
		const auto& components = Item.find("$_components");

		if (components == Item.end())
		{
			SF_EMIT_WARNING("Object #{} was missing it's '$_components' key, skipping", ItemIndex);

			return nullptr;
		}

		GameObject* object = GameObject::Create();

		for (auto it = components.value().begin(); it != components.value().end(); it++)
		{
			std::string name = it.value();

			if (EntityComponent type = s_ComponentNameToType.at(name); !object->GetComponentByType(type))
				object->AddComponent(type);
		}

		return object;
	}
}

static std::vector<GameObjectRef> LoadMapVersion2(const std::string& Contents, float Version, bool* Success)
{
	ZoneScoped;

	nlohmann::json jsonData;

	try
	{
		ZoneScopedN("ParseJson");
		jsonData = nlohmann::json::parse(Contents);
	}
	catch (const nlohmann::json::parse_error& e)
	{
		errorString = std::format(
			"V2 - JSON Parsing error: {}",
			e.what()
		);

		*Success = false;

		return {};
	}

	std::string sceneName = jsonData.value("SceneName", "<UNNAMED SCENE>");

	if (jsonData.find("GameObjects") == jsonData.end())
	{
		errorString = std::format(
			"The `GameObjects` key is not present in scene '{}'",
			sceneName
		);

		*Success = false;

		return {};
	}

	nlohmann::json gameObjectsNode = jsonData["GameObjects"];

	std::unordered_map<int64_t, GameObjectRef> objectsMap;
	std::unordered_map<int64_t, int64_t> realIdToSceneId;
	std::unordered_map<uint32_t, std::unordered_map<std::string, uint32_t>> objectProps;

	objectsMap.reserve(gameObjectsNode.size());
	realIdToSceneId.reserve(gameObjectsNode.size());

	for (uint32_t itemIndex = 0; itemIndex < gameObjectsNode.size(); itemIndex++)
	{
		ZoneScopedN("DeserializeItem");

		const nlohmann::json& item = gameObjectsNode[itemIndex];

		GameObject* newObject = createObjectFromJsonItem(item, itemIndex, Version);

		std::string name = item.find("Name") != item.end() ? (std::string)item["Name"] : newObject->Name;

		if (item.find("$_objectId") == item.end())
			SF_EMIT_WARNING("Object #{} was missing it's '$_objectId' key", itemIndex);
		else
		{
			uint32_t itemObjectId = item["$_objectId"];

			if (const auto& prev = objectsMap.find(itemObjectId); prev != objectsMap.end())
			{
				SF_EMIT_WARNING(
					"Object #{} ('{}') shares an `$_objectId` ({}) with ID:{} ('{}'), it will be skipped",
					itemIndex, item.value("Name", "<UNNAMED>"), itemObjectId, prev->second->ObjectId, prev->second->Name
				);
				continue;
			}

			objectsMap.insert(std::pair(itemObjectId, newObject));
			realIdToSceneId.insert(std::pair(newObject->ObjectId, itemObjectId));
		}

		objectProps[newObject->ObjectId] = {};

		ZoneName("DeserializeProperties", 21);

		// https://json.nlohmann.me/features/iterators/#access-object-key-during-iteration
		for (auto memberIt = item.begin(); memberIt != item.end(); ++memberIt)
		{
			std::string memberName = memberIt.key();

			// reserved prefix for data which needs to be saved but isn't a property
			if (memberName[0] == '$' && memberName[1] == '_')
				continue;

			nlohmann::json memberValue = memberIt.value();

			Reflection::Property* member = newObject->FindProperty(memberName);

			if (!member)
			{
				SF_EMIT_WARNING(
					"Member '{}' is not defined in the API (Name: '{}')!",
					memberName,
					name
				);
				continue;
			}

			Reflection::ValueType memberType = member->Type;

			if (!member->Set)
			{
				SF_EMIT_WARNING(
					"Member '{}' of '{}' is read-only!",
					memberName,
					name
				);
				continue;
			}

			Reflection::GenericValue assignment;

			switch (memberType)
			{

			case Reflection::ValueType::String:
			{
				assignment = (std::string)memberValue;

				break;
			}

			case Reflection::ValueType::Boolean:
			{
				assignment = (bool)memberValue;

				break;
			}

			case Reflection::ValueType::Double:
			{
				assignment = (double)memberValue;

				break;
			}

			case Reflection::ValueType::Integer:
			{
				assignment = (int)memberValue;

				break;
			}

			case Reflection::ValueType::Color:
			{
				assignment = GetColorFromJson(memberValue).ToGenericValue();

				break;
			}

			case Reflection::ValueType::Vector3:
			{
				assignment = GetVector3FromJson(memberValue);

				break;
			}

			case Reflection::ValueType::Matrix:
			{
				assignment = GetMatrixFromJson(memberValue);

				break;
			}

			case Reflection::ValueType::GameObject:
			{
				objectProps[newObject->ObjectId].insert(std::pair(memberName, memberValue));

				break;
			}

			default:
			{
				const std::string_view& memberTypeName = Reflection::TypeAsString(memberType);

				SF_EMIT_WARNING(
					"Not reading prop '{}' because it's type ({}) is unknown",
					memberName,
					memberTypeName
				);

				break;
			}

			}

			if (assignment.Type != Reflection::ValueType::Null)
			{
				try
				{
					newObject->SetPropertyValue(memberName, assignment);
				}
				catch (std::string err)
				{
					const std::string_view& mtname = Reflection::TypeAsString(memberType);
					std::string valueStr = assignment.ToString();

					SF_EMIT_WARNING(
						"Failed to set {} Property '{}' of '{}' to '{}': {}",
						mtname, memberName, name, valueStr, err
					);
				}
			}
		}
	}

	std::vector<GameObjectRef> objects;
	objects.reserve(objectsMap.size());

	ZoneNamedN(fixupzone, "FixupObjectReferentProperties", true);

	for (auto& it : objectsMap)
	{
		GameObjectRef object = it.second;

		// !! IMPORTANT !!
		// The `Parent` key *should not* be set for Root Nodes as their parent
		// *is not part of the scene!*
		// 04/09/2024
		if (objectProps[object->ObjectId].find("Parent") == objectProps[object->ObjectId].end())
			objects.push_back(object);

		for (auto& objectProp : objectProps[object->ObjectId])
		{
			const std::string_view& propName = objectProp.first;
			const uint32_t sceneRelativeId = objectProp.second;

			auto target = objectsMap.find(sceneRelativeId);

			if (target != objectsMap.end())
			{
				try
				{
					object->SetPropertyValue(propName, target->second->ToGenericValue());
				}
				catch (std::string err)
				{
					std::string valueStr = target->second->GetFullName();

					SF_EMIT_WARNING(
						"Failed to set GameObject property of '{}' to '{}': {}",
						object->Name, valueStr, err
					);
				}
			}
			else
			{
				SF_EMIT_WARNING(
					"'{}' refers to invalid scene-relative Object ID {} for prop {}. To avoid UB, it will be NULL'd.",
					object->Name,
					sceneRelativeId,
					propName
				);

				Reflection::GenericValue gv{ PHX_GAMEOBJECT_NULL_ID };
				gv.Type = Reflection::ValueType::GameObject;

				object->SetPropertyValue(propName, gv);
			}
		}
	}

	return objects;
}

std::vector<GameObjectRef> SceneFormat::Deserialize(
	const std::string& Contents,
	bool* SuccessPtr
)
{
	ZoneScoped;

	float version = getVersion(Contents);

	size_t jsonStartLoc = Contents.find_first_of("{");

	if (jsonStartLoc == std::string::npos)
	{
		errorString = std::format(
			"Unable to find JSON section of file. Format version retrieved was {}",
			version
		);
		*SuccessPtr = false;

		return {};
	}

	std::string jsonFileContents = Contents.substr(jsonStartLoc);

	std::vector<GameObjectRef> objects{};

	if (version >= 1.f && version < 2.f)
		objects = LoadMapVersion1(jsonFileContents, SuccessPtr);

	else if (version >= 2.f && version < 3.f)
			objects = LoadMapVersion2(jsonFileContents, version, SuccessPtr);

	else
	{
		errorString = std::format(
			"Format version '{}' not recognized",
			version
		);
		*SuccessPtr = false;

		return {};
	}

	return objects;
}

static nlohmann::json serializeObject(GameObject* Object, bool IsRootNode = false)
{
	ZoneScoped;

	nlohmann::json item{};
	item["$_components"] = nlohmann::json::array();

	for (const std::pair<EntityComponent, uint32_t>& handle : Object->GetComponents())
		item["$_components"].push_back(s_EntityComponentNames[(size_t)handle.first]);

	for (const auto& prop : Object->GetProperties())
	{
		const std::string_view& propName = prop.first;
		const Reflection::Property& propInfo = prop.second;

		if (!propInfo.Set && propName != "ObjectId")
			continue;

		// !! IMPORTANT !!
		// The `Parent` key *should not* be set for Root Nodes as their parent
		// *is not part of the scene!*
		// 04/09/2024
		if (IsRootNode && propName == "Parent")
			continue;

		std::string_view serializedAs = propName;

		if (serializedAs == "ObjectId")
			// it's read-only but still needs to be stored to preserve hierarchy information
			serializedAs = "$_objectId";

		Reflection::GenericValue value = Object->GetPropertyValue(propName);

		switch (prop.second.Type)
		{
		
		case Reflection::ValueType::Boolean:
		{
			item[serializedAs] = value.AsBoolean();
			break;
		}
		case Reflection::ValueType::Integer:
		{
			item[serializedAs] = value.AsInteger();
			break;
		}
		case Reflection::ValueType::Double:
		{
			item[serializedAs] = value.AsDouble();
			break;
		}
		case Reflection::ValueType::String:
		{
			item[serializedAs] = value.AsStringView();
			break;
		}
		
		case Reflection::ValueType::Color:
		{
			Color col(value);
			item[serializedAs] = { col.R, col.G, col.B };
			break;
		}
		case Reflection::ValueType::Vector2:
		{
			const glm::vec2 vec = value.AsVector2();
			item[serializedAs] = { vec.x, vec.y };
			break;
		}
		case Reflection::ValueType::Vector3:
		{
			glm::vec3& vec = value.AsVector3();
			item[serializedAs] = { vec.x, vec.y, vec.z };
			break;
		}
		case Reflection::ValueType::Matrix:
		{
			glm::mat4 mat = value.AsMatrix();
			item[serializedAs] = nlohmann::json::array();

			for (int col = 0; col < 4; col++)
			{
				item[serializedAs][col] = nlohmann::json::array();

				for (int row = 0; row < 4; row++)
					item[serializedAs][col][row] = mat[col][row];
			}
				
			break;
		}

		case Reflection::ValueType::GameObject:
		{
			GameObject* target = GameObject::FromGenericValue(value);

			if (target)
				item[serializedAs] = target->ObjectId;
			else
				item[serializedAs] = PHX_GAMEOBJECT_NULL_ID;
			
			break;
		}

		[[unlikely]] default:
		{
			assert(false);
		}

		}
	}

	return item;
}

std::string SceneFormat::Serialize(std::vector<GameObject*> Objects, const std::string& SceneName)
{
	ZoneScoped;

	nlohmann::json json;

	json["SceneName"] = SceneName;

	nlohmann::json objectsArray = nlohmann::json::array();

	for (GameObject* rootObject : Objects)
	{
		objectsArray.push_back(serializeObject(rootObject, true));
		
		for (GameObject* desc : rootObject->GetDescendants())
			if (desc->Serializes)
				objectsArray.push_back(serializeObject(desc));
	}

	json["GameObjects"] = objectsArray;

	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::chrono::year_month_day ymd = std::chrono::floor<std::chrono::days>(now);


	std::string dateStr = std::to_string((uint32_t)ymd.day()) + "-"
							+ std::to_string((uint32_t)ymd.month()) + "-"
							+ std::to_string((int32_t)ymd.year());
	
	std::string contents = std::string("PHNXENGI\n")
							+ "#Version 2.10\n"
							+ "#Asset Scene\n"
							+ "#Date " + dateStr + "\n"
							+ "#SceneName " + SceneName + "\n"
							+ "\n"
							+ json.dump(2);

	return contents;
}

std::string SceneFormat::GetLastErrorString()
{
	return errorString;
}
