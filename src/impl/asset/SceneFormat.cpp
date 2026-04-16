// TODO: cleanup code!

#include <chrono>
#include <nljson.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "asset/SceneFormat.hpp"

#include "asset/PrimitiveMeshes.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/ModelImporter.hpp"
#include "datatype/ComponentDependencies.hpp"
#include "component/Transform.hpp"
#include "component/RigidBody.hpp"
#include "component/Light.hpp"
#include "component/Mesh.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

#define SF_WARN(err, ...) Log.WarningF( \
	"Deserialization warning: " err,    \
	__VA_ARGS__                         \
)                                       \

thread_local std::string errorString = "No error";

static auto loadModelAsMeshes(
	const char* ModelFilePath,
	glm::vec3 Size,
	glm::vec3 Position,
	bool AutoParent = true
)
{
	ModelLoader Loader = ModelLoader(ModelFilePath, AutoParent ? GameObjectManager::Get()->DataModel : PHX_GAMEOBJECT_NULL_ID);

	for (GameObject* object : Loader.LoadedObjs)
	{
		EcTransform* mesh = object->FindComponent<EcTransform>();

		if (mesh)
		{
			mesh->Transform = glm::translate(mesh->Transform, (glm::vec3)Position);
			mesh->Size = mesh->Size * Size;
		}
	}

	return Loader.LoadedObjs;
}

static glm::vec2 getVector2FromJson(const nlohmann::json& Json)
{
	glm::vec2 Vec2;

	try
	{
		Vec2 = glm::vec2(
			Json[0],
			Json[1]
		);
	}
	catch (const nlohmann::json::type_error& TErr)
	{
		Log.WarningF(
			"Could not read Vector2: {}",
			TErr.what()
		);
	}
	

	return Vec2;
}

static glm::vec3 getVector3FromJson(const nlohmann::json& Json)
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
		Log.WarningF(
			"Could not read Vector3: {}",
			TErr.what()
		);
	}

	return Vec3;
}

static Color getColorFromJson(const nlohmann::json& Json)
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
		Log.WarningF(
			"Could not read Color: {}",
			TErr.what()
		);
	}

	return col;
}

static glm::mat4 getMatrixFromJson(const nlohmann::json& Json)
{
	glm::mat4 mat;

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
		Log.WarningF(
			"Could not read Matrix: {}",
			TErr.what()
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

static std::vector<ObjectRef> loadSceneVersion1(
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

	const nlohmann::json& PartsNode = JsonData["parts"];
	const nlohmann::json& ModelsNode = JsonData.value("props", nlohmann::json{});
	const nlohmann::json& LightsNode = JsonData["lights"];

	std::vector<ObjectRef> Objects;
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
			RAISE_RTF("Failed to decode map data: {}", e.what());
		}

		std::string ModelPath = PropObject["path"];

		glm::vec3 Position = getVector3FromJson(PropObject["position"]);
		//Vector3 Orientation = GetVector3FromJson(PropObject["orient"]);
		glm::vec3 Size = getVector3FromJson(PropObject["size"]);

		std::vector<ObjectRef> Model = loadModelAsMeshes(ModelPath.c_str(), Size, Position);

		std::string modelName = PropObject.value("name", "<UN-NAMED>");

		if (Model.empty())
			Log.Warning(
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
				GameObject* container = GameObjectManager::s_Create("Model");

				for (size_t index = 0; index < Model.size(); index++)
				{
					GameObject* mesh = Model[index].Referred();
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
			auto prop_3d = obj->FindComponent<EcMesh>();

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

			if (EcRigidBody* crb = obj->FindComponent<EcRigidBody>())
				crb->PhysicsDynamics = PropObject.value("computePhysics", 0) == 1 ? true : false;
		}
	}

	MaterialManager* mtlManager = MaterialManager::Get();

	for (uint32_t Index = 0; Index < PartsNode.size(); Index++)
	{
		const nlohmann::json& Object = PartsNode[Index];

		GameObject* NewObject = GameObjectManager::s_Create("Primitive");
		Objects.push_back(NewObject);

		EcTransform* ct = NewObject->FindComponent<EcTransform>();
		EcMesh* cm = NewObject->FindComponent<EcMesh>();

		NewObject->Name = Object.value("name", NewObject->Name);

		glm::vec3 Position = getVector3FromJson(Object["position"]);
		glm::vec3 Orientation{};

		if (Object.find("orient") != Object.end())
		{
			Orientation = getVector3FromJson(Object["orient"]);
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
			cm->Tint = getColorFromJson(Object["color"]);

		ct->Size = getVector3FromJson(Object["size"]);

		if (Object.find("material") != Object.end())
		{
			uint32_t MeshMaterial = mtlManager->LoadFromPath(std::string(Object["material"]));

			cm->MaterialId = MeshMaterial;

		}
		else if (Object.find("textures") != Object.end())
		{

		}

		if (!NewObject->FindComponent<EcRigidBody>())
			NewObject->AddComponent(EntityComponent::RigidBody);

		EcRigidBody* crb = NewObject->FindComponent<EcRigidBody>();
		crb->PhysicsDynamics = Object.value("computePhysics", 0) == 1 ? true : false;
	}

	for (uint32_t LightIndex = 0; LightIndex < LightsNode.size(); LightIndex++)
	{
		const nlohmann::json& LightObject = LightsNode[LightIndex];

		std::string LightType = LightObject["type"];

		GameObject* Object = GameObjectManager::s_Create(LightType);
		Objects.push_back(Object);

		if (LightType != "DirectionalLight")
		{
			EcTransform* ct = Object->FindComponent<EcTransform>();

			ct->Transform = glm::translate(
				glm::mat4(1.f),
				(glm::vec3)getVector3FromJson(LightObject["position"])
			);
		}
		else
		{
			Object->FindComponent<EcDirectionalLight>()->Direction = getVector3FromJson(LightObject["position"]);
		}

		//Light->LightColor = GetColorFromJson(LightObject["color"]);

		if (LightType == "PointLight")
		{
			EcPointLight* Pointlight = Object->FindComponent<EcPointLight>();
			Pointlight->Range = LightObject["range"];
		}
	}

	return Objects;
}

// For Versions < 2.12, certain components would automatically add other components
// Such as `Mesh` adding `Transform`. Replicate this behaviour for older scenes
static void addLegacyComponentDependencies(GameObject* object)
{
	for (const ReflectorRef& r : object->Components)
	{
		for (EntityComponent ecx : GetCommonDependenciesForComponent(r.Type))
		{
			if (!object->FindComponentByType(ecx))
				object->AddComponent(ecx);
		}
	}
}

static ObjectRef createObjectFromJsonItem(const nlohmann::json& Item, uint32_t ItemIndex, float Version)
{
	if (Version == 2.f)
	{
		const auto& classNameJson = Item.find("$_class");

		if (classNameJson == Item.end())
		{
			SF_WARN("Object #{} was missing it's '$_class' key, skipping", ItemIndex);

			return nullptr;
		}

		std::string className = classNameJson.value();
		if (className == "Primitive")
			className = "Mesh";

		GameObject* object = GameObjectManager::s_Create(className);
		addLegacyComponentDependencies(object);

		return object;
	}
	else
	{
		const auto& components = Item.find("$_components");

		if (components == Item.end())
		{
			SF_WARN("Object #{} was missing it's '$_components' key, skipping", ItemIndex);

			return nullptr;
		}

		ObjectRef object = GameObjectManager::Get()->Create();

		for (auto it = components.value().begin(); it != components.value().end(); it++)
		{
			std::string name = it.value();
			EntityComponent ec = FindComponentTypeByName(name);

			if (ec == EntityComponent::None)
			{
				SF_WARN("Object #{} with name '{}' had invalid component '{}'", ItemIndex, Item.value("Name", "<UNNAMED>"), name);
				continue;
			}

			if (!object->FindComponentByType(ec))
				object->AddComponent(ec);
		}

		if (Version < 2.11f)
		{
			if (object->FindComponent<EcMesh>() && !object->FindComponent<EcRigidBody>())
				object->AddComponent(EntityComponent::RigidBody);
		}

		if (Version < 2.12f)
			addLegacyComponentDependencies(object);

		if (const auto& tagIt = Item.find("$_tags"); tagIt != Item.end())
		{
			for (size_t i = 0; i < tagIt.value().size(); i++)
				object->AddTag(tagIt.value()[i]);
		}

		return object;
	}
}

static std::vector<ObjectRef> loadSceneVersion2(const std::string& Contents, float Version, bool* Success)
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

	std::string_view sceneName = jsonData.value("SceneName", "<UNNAMED SCENE>");

	if (jsonData.find("GameObjects") == jsonData.end())
	{
		errorString = std::format(
			"The `GameObjects` key is not present in scene '{}'",
			sceneName
		);

		*Success = false;

		return {};
	}

	const nlohmann::json& gameObjectsNode = jsonData["GameObjects"];

	std::vector<std::pair<int64_t,  ObjectRef>> objects;
	std::unordered_map<int64_t, ObjectRef> objectsMap;
	std::unordered_map<int64_t, int64_t> realIdToSceneId;
	std::unordered_map<uint32_t, std::unordered_map<std::string, uint32_t>> objectProps;

	objects.reserve(gameObjectsNode.size());
	realIdToSceneId.reserve(gameObjectsNode.size());

	for (uint32_t itemIndex = 0; itemIndex < gameObjectsNode.size(); itemIndex++)
	{
		const nlohmann::json& item = gameObjectsNode[itemIndex];

		ObjectRef newObject = createObjectFromJsonItem(item, itemIndex, Version);

		std::string name = item.find("Name") != item.end() ? (std::string)item["Name"] : newObject->Name;

		if (item.find("$_objectId") == item.end())
			SF_WARN("Object #{} was missing it's '$_objectId' key", itemIndex);
		else
		{
			uint32_t itemObjectId = item["$_objectId"];

			if (const auto& prev = objectsMap.find(itemObjectId); prev != objectsMap.end())
			{
				SF_WARN(
					"Object #{} ('{}') shares an `$_objectId` ({}) with ID:{} ('{}'), it will be skipped",
					itemIndex, item.value("Name", "<UNNAMED>"), itemObjectId, prev->second->ObjectId, prev->second->Name
				);
				continue;
			}

			objectsMap.insert(std::pair(itemObjectId, newObject));
			objects.push_back(std::pair(itemObjectId, newObject));
			realIdToSceneId.insert(std::pair(newObject->ObjectId, itemObjectId));
		}

		objectProps[newObject->ObjectId] = {};

		// https://json.nlohmann.me/features/iterators/#access-object-key-during-iteration
		for (auto propIt = item.begin(); propIt != item.end(); propIt++)
		{
			std::string_view propName = propIt.key();

			// reserved prefix for data which needs to be saved but isn't a property
			if (propName[0] == '$' && propName[1] == '_')
				continue;

			if (Version < 2.11f && propName == "Asset")
				propName = "MeshAsset";

			if (Version < 2.13f && propName == "MetallnessFactor")
				propName = "MetalnessFactor";

			const nlohmann::json& memberValue = propIt.value();

			const Reflection::PropertyDescriptor* prop = newObject->FindProperty(propName);

			if (!prop)
			{
				SF_WARN(
					"Member '{}' is not defined in the API (Name: '{}')!",
					propName,
					name
				);
				continue;
			}

			if (!prop->Set)
			{
				SF_WARN(
					"Member '{}' of '{}' is read-only!",
					propName,
					name
				);
				continue;
			}

			Reflection::ValueType propType = prop->Type;
			propType = Reflection::ValueType(propType & ~Reflection::ValueType::Null);

			Reflection::GenericValue assignment;

			switch (propType)
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
				assignment = getColorFromJson(memberValue).ToGenericValue();

				break;
			}

			case Reflection::ValueType::Vector2:
			{
				assignment = getVector2FromJson(memberValue);
				
				break;
			}

			case Reflection::ValueType::Vector3:
			{
				assignment = getVector3FromJson(memberValue);

				break;
			}

			case Reflection::ValueType::Matrix:
			{
				assignment = getMatrixFromJson(memberValue);

				break;
			}

			case Reflection::ValueType::GameObject:
			{
				objectProps[newObject->ObjectId].insert(std::pair(
					propName,
					memberValue.is_null() ? PHX_GAMEOBJECT_NULL_ID : (uint32_t)memberValue
				));

				break;
			}

			default:
			{
				SF_WARN(
					"Property '{}' cannot be deserialized because it's type ({}) is not supported",
					propName,
					Reflection::TypeAsString(propType)
				);

				break;
			}

			}

			if (assignment.Type != Reflection::ValueType::Null)
			{
				try
				{
					newObject->SetPropertyValue(propName, assignment);
				}
				catch (const std::runtime_error& err)
				{
					SF_WARN(
						"Failed to set {} Property '{}' of '{}' to '{}': {}",
						Reflection::TypeAsString(propType), propName, name, assignment.ToString(), err.what()
					);
				}
			}
		}
	}

	ZoneNamedN(fixupzone, "FixupObjectReferentProperties", true);

	std::vector<ObjectRef> rootObjects;

	for (auto& it : objects)
	{
		ObjectRef object = it.second;

		// !! IMPORTANT !!
		// The `Parent` key *should not* be set for Root Nodes as their parent
		// *is not part of the scene!*
		// 04/09/2024
		if (objectProps[object->ObjectId].find("Parent") == objectProps[object->ObjectId].end())
			rootObjects.push_back(object);

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
				catch (const std::runtime_error& err)
				{
					std::string valueStr = target->second->GetFullName();

					SF_WARN(
						"Failed to set GameObject property of '{}' to '{}': {}",
						object->Name, valueStr, err.what()
					);
				}
			}
			else
			{
				SF_WARN(
					"'{}' refers to invalid scene-relative Object ID {} for property {}",
					object->Name,
					sceneRelativeId,
					propName
				);

				object->SetPropertyValue(propName, GameObject::s_ToGenericValue(nullptr));
			}
		}
	}

	return rootObjects;
}

std::vector<ObjectRef> SceneFormat::Deserialize(
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

	std::vector<ObjectRef> objects{};

	if (version >= 1.f && version < 2.f)
		objects = loadSceneVersion1(jsonFileContents, SuccessPtr);

	else if (version >= 2.f && version < 3.f)
			objects = loadSceneVersion2(jsonFileContents, version, SuccessPtr);

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

struct Serializer
{
	void SerializeObject(GameObject* Object, bool IsRootNode = false);
	uint32_t IdCounter = 0;
	std::unordered_map<uint32_t, uint32_t> RealIdToSceneId;
	std::unordered_map<uint32_t, std::vector<std::pair<uint32_t, std::string_view>>> PendingIdReplacement;
	nlohmann::json Items;
};

void Serializer::SerializeObject(GameObject* Object, bool IsRootNode)
{
	ZoneScoped;

	nlohmann::json item;
	item["$_components"] = nlohmann::json::array();
	item["$_objectId"] = IdCounter;
	RealIdToSceneId[Object->ObjectId] = IdCounter;

	if (const auto& it = PendingIdReplacement.find(Object->ObjectId); it != PendingIdReplacement.end())
	{
		for (const std::pair<uint32_t, std::string_view>& replacement : it->second)
			Items[replacement.first][replacement.second] = IdCounter;
	}

	IdCounter++;

	for (const ReflectorRef& handle : Object->Components)
		item["$_components"].push_back(s_EntityComponentNames[(size_t)handle.Type]);

	for (const auto& prop : Object->GetProperties())
	{
		const std::string_view& propName = prop.first;
		const Reflection::PropertyDescriptor* propInfo = prop.second;

		if (!propInfo->Serializes)
			continue;

		if (!propInfo->Set)
			continue;

		// !! IMPORTANT !!
		// The `Parent` key *should not* be set for Root Nodes as their parent
		// *is not part of the scene!*
		// 04/09/2024
		if (IsRootNode && propName == "Parent")
			continue;

		Reflection::GenericValue value = Object->GetPropertyValue(propName);

		if (Object->GetDefaultPropertyValue(propName) == value)
			continue; // don't serialize properties that haven't changed

		switch (value.Type)
		{
		
		case Reflection::ValueType::Boolean:
		{
			item[propName] = value.AsBoolean();
			break;
		}
		case Reflection::ValueType::Integer:
		{
			item[propName] = value.AsInteger();
			break;
		}
		case Reflection::ValueType::Double:
		{
			item[propName] = value.AsDouble();
			break;
		}
		case Reflection::ValueType::String:
		{
			item[propName] = value.AsStringView();
			break;
		}
		
		case Reflection::ValueType::Color:
		{
			Color col = Color(value);
			item[propName] = { col.R, col.G, col.B };
			break;
		}
		case Reflection::ValueType::Vector2:
		{
			const glm::vec2 vec = value.AsVector2();
			item[propName] = { vec.x, vec.y };
			break;
		}
		case Reflection::ValueType::Vector3:
		{
			glm::vec3& vec = value.AsVector3();
			item[propName] = { vec.x, vec.y, vec.z };
			break;
		}
		case Reflection::ValueType::Matrix:
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

		case Reflection::ValueType::GameObject:
		{
			GameObject* target = GameObjectManager::Get()->FromGenericValue(value);

			if (target && !target->IsDestructionPending)
			{
				const auto& it = RealIdToSceneId.find(target->ObjectId);
				if (it != RealIdToSceneId.end())
					item[propName] = it->second;
				else
				{
					item[propName] = PHX_GAMEOBJECT_NULL_ID;
					std::vector<std::pair<uint32_t, std::string_view>>& replacements = PendingIdReplacement[target->ObjectId];
					replacements.push_back(std::pair(Items.size(), propName));
				}
			}
			else
				item[propName] = PHX_GAMEOBJECT_NULL_ID;
			
			break;
		}

		case Reflection::ValueType::Null:
		{
			item[propName] = nlohmann::json(); // save as Null
			break;
		}

		[[unlikely]] default:
		{
			assert(false);
			Log.ErrorF(
				"Cannot serialize property '{}' of {} because it has unserializable type {}", 
				propName,
				Object->GetFullName(),
				Reflection::TypeAsString(value.Type)
			);
		}

		}
	}

	if (Object->Tags.size() > 0)
	{
		item["$_tags"] = nlohmann::json::array();
		for (uint16_t tagId : Object->Tags)
			item["$_tags"].push_back(GameObjectManager::Get()->Collections[tagId].Name);
	}

	Items.push_back(item);
}

std::string SceneFormat::Serialize(std::vector<GameObject*> Objects, const std::string& SceneName)
{
	ZoneScoped;

	nlohmann::json json;
	json["SceneName"] = SceneName;

	Serializer serializer;

	for (GameObject* rootObject : Objects)
	{
		serializer.SerializeObject(rootObject, /* IsRootNode = */ true);
		
		for (GameObject* desc : rootObject->GetDescendants())
			if (desc->Serializes)
				serializer.SerializeObject(desc);
	}

	json["GameObjects"] = serializer.Items;

	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::chrono::year_month_day ymd = std::chrono::floor<std::chrono::days>(now);


	std::string dateStr = std::to_string((uint32_t)ymd.day()) + "-"
							+ std::to_string((uint32_t)ymd.month()) + "-"
							+ std::to_string((int32_t)ymd.year());
	
	std::string contents = std::string("PHOENIXF\n")
							+ "#Version 2.13\n"
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
