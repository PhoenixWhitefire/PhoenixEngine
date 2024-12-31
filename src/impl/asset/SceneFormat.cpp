// TODO: cleanup code!

#include <nljson.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "asset/SceneFormat.hpp"

#include "asset/PrimitiveMeshes.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/ModelImporter.hpp"
#include "gameobject/Light.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

static std::string errorString = "No error";

static std::unordered_map<std::string, std::string> SpecialPropertyToSerializedName =
{
	{ "ObjectId", "$_objectId" },
	{ "Class", "$_class" }
};

static std::vector<std::string> SpecialSerializedNames =
{
	"$_objectId",
	"$_class"
};

static auto LoadModelAsMeshes(
	const char* ModelFilePath,
	Vector3 Size,
	Vector3 Position,
	bool AutoParent = true
)
{
	ModelLoader Loader(ModelFilePath, AutoParent ? static_cast<GameObject*>(GameObject::s_DataModel) : nullptr);

	for (GameObject* object : Loader.LoadedObjs)
	{
		Object_Mesh* mesh = static_cast<Object_Mesh*>(object);

		mesh->Transform = glm::translate(mesh->Transform, (glm::vec3)Position);
		mesh->Size = mesh->Size * Size;
	}

	return Loader.LoadedObjs;
}

static Vector3 GetVector3FromJson(const nlohmann::json& Json)
{
	Vector3 Vec3;

	try
	{
		Vec3 = Vector3(
			Json[0],
			Json[1],
			Json[2]
		);
	}
	catch (nlohmann::json::type_error TErr)
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
	catch (nlohmann::json::type_error TErr)
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
	catch (nlohmann::json::type_error TErr)
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
	Objects.reserve(PartsNode.size() + ModelsNode.size() + LightsNode.size());

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

		if (Model.empty())
			Log::Warning(
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

		if (Model.size() >= 1)
		{
			auto prop_3d = static_cast<Object_Base3D*>(Model[0]);

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

		if (Object["type"] == "MeshPart")
		{
			// Yeah, "MeshPart" as the type and `Primitive`s as the result.
			// Like just use V2 bro why the hell you botherin' this musty-ahh
			// format?
			GameObject* NewObject = GameObject::Create("Primitive");
			Objects.push_back(NewObject);

			Object_Base3D* Object3D = static_cast<Object_Base3D*>(NewObject);

			NewObject->Name = Object.value("name", NewObject->Name);

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

			// YXZ Rotation order
			Object3D->Transform = glm::rotate(
				Object3D->Transform,
				glm::radians(static_cast<float>(Orientation.Y)),
				glm::vec3(0.0f, 1.0f, 0.0f)
			);
			Object3D->Transform = glm::rotate(
				Object3D->Transform,
				glm::radians(static_cast<float>(Orientation.X)),
				glm::vec3(1.0f, 0.0f, 0.0f)
			);
			Object3D->Transform = glm::rotate(
				Object3D->Transform,
				glm::radians(static_cast<float>(Orientation.Z)),
				glm::vec3(0.0f, 0.0f, 1.0f)
			);

			if (Object.find("color") != Object.end())
				Object3D->Tint = GetColorFromJson(Object["color"]);

			Object3D->Size = GetVector3FromJson(Object["size"]);

			if (Object.find("material") != Object.end())
			{
				uint32_t MeshMaterial = mtlManager->LoadMaterialFromPath(Object["material"]);

				Object3D->MaterialId = MeshMaterial;

			}
			else if (Object.find("textures") != Object.end())
			{

			}

			Object3D->PhysicsDynamics = Object.value("computePhysics", 0) == 1 ? true : false;
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

		GameObject* Object = (GameObject::Create(LightType));
		Objects.push_back(Object);

		Object_Light* Light = static_cast<Object_Light*>(Object);

		Light->LocalTransform = glm::translate(
			glm::mat4(1.f),
			(glm::vec3)GetVector3FromJson(LightObject["position"])
		);

		Light->LightColor = GetColorFromJson(LightObject["color"]);

		if (LightType == "PointLight")
		{
			Object_PointLight* Pointlight = static_cast<Object_PointLight*>(Object);
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

	objectsMap.reserve(GameObjectsNode.size());
	realIdToSceneId.reserve(GameObjectsNode.size());

	for (uint32_t itemIndex = 0; itemIndex < GameObjectsNode.size(); itemIndex++)
	{
		const nlohmann::json& item = GameObjectsNode[itemIndex];

		if (item.find("$_class") == item.end())
		{
			const char* fmtStr = "Deserialization warning: Object #{} was missing it's '$_class' key";
			auto fmtArgs = std::make_format_args(itemIndex);
			Log::Warning(std::vformat(fmtStr, fmtArgs));

			continue;
		}

		std::string className = item["$_class"];
		std::string name = item.find("Name") != item.end() ? (std::string)item["Name"] : className;

		GameObject* newObject = GameObject::Create(className);

		if (item.find("$_objectId") == item.end())
		{
			Log::Warning(std::vformat(
				"Deserialization warning: Object #{} ({}) was missing it's '$_objectId' key",
				std::make_format_args(itemIndex, className)
			));

			continue;
		}

		uint32_t itemObjectId = item["$_objectId"];

		objectsMap.insert(std::pair(itemObjectId, newObject));
		realIdToSceneId.insert(std::pair(newObject->ObjectId, itemObjectId));

		objectProps.insert(std::pair(newObject, std::unordered_map<std::string, uint32_t>{}));

		// https://json.nlohmann.me/features/iterators/#access-object-key-during-iteration
		for (auto memberIt = item.begin(); memberIt != item.end(); ++memberIt)
		{
			std::string memberName = memberIt.key();

			if (std::find(
				SpecialSerializedNames.begin(),
				SpecialSerializedNames.end(),
				memberName
			) != SpecialSerializedNames.end()
				)
				continue;

			nlohmann::json memberValue = memberIt.value();

			bool hasProp = newObject->HasProperty(memberName);

			if (!hasProp)
			{
				const char* fmtStr = "Deserialization warning: Member '{}' is not defined in the API for the Class {} (Name: '{}')!";
				auto fmtArgs = std::make_format_args(
					memberName,
					className,
					name
				);
				Log::Warning(std::vformat(fmtStr, fmtArgs));

				continue;
			}

			auto& member = newObject->GetProperty(memberName);

			Reflection::ValueType memberType = member.Type;
			auto& setProperty = member.Set;

			if (!setProperty)
			{
				const char* fmtStr = "Deserialization warning: Member '{}' of {} '{}' is read-only!";
				auto fmtArgs = std::make_format_args(
					memberName,
					className,
					name
				);
				Log::Warning(std::vformat(fmtStr, fmtArgs));

				continue;
			}

			Reflection::GenericValue assignment;

			switch (memberType)
			{

			case (Reflection::ValueType::String):
			{
				assignment = (std::string)memberValue;

				break;
			}

			case (Reflection::ValueType::Bool):
			{
				assignment = (bool)memberValue;

				break;
			}

			case (Reflection::ValueType::Double):
			{
				assignment = (double)memberValue;

				break;
			}

			case (Reflection::ValueType::Integer):
			{
				assignment = (int)memberValue;

				break;
			}

			case (Reflection::ValueType::Color):
			{
				assignment = GetColorFromJson(memberValue).ToGenericValue();

				break;
			}

			case (Reflection::ValueType::Vector3):
			{
				assignment = GetVector3FromJson(memberValue).ToGenericValue();

				break;
			}

			case (Reflection::ValueType::Matrix):
			{
				assignment = GetMatrixFromJson(memberValue);

				break;
			}

			case (Reflection::ValueType::GameObject):
			{
				objectProps[newObject].insert(std::pair(memberName, memberValue));

				break;
			}

			default:
			{
				const char* fmtStr = "Deserialization warning: Not reading prop '{}' of class {} because it's type ({}) is unknown";
				const std::string& memberTypeName = Reflection::TypeAsString(memberType);

				auto fmtArgs = std::make_format_args(
					memberName,
					className,
					memberTypeName
				);

				Log::Warning(std::vformat(fmtStr, fmtArgs));

				break;
			}

			}

			if (assignment.Type != Reflection::ValueType::Null)
			{
				try
				{
					setProperty(newObject, assignment);
				}
				catch (std::string err)
				{
					std::string mtname = Reflection::TypeAsString(memberType);
					std::string valueStr = assignment.ToString();

					Log::Warning(std::vformat(
						"Deserialization warning: Failed to set {} Property '{}' of '{}' ({}) to '{}': {}",
						std::make_format_args(mtname, memberName, name, className, valueStr, err)
					));
				}
			}
		}
	}

	std::vector<GameObject*> objects;
	objects.reserve(objectsMap.size());

	for (auto& it : objectsMap)
	{
		GameObject* object = it.second;

		// !! IMPORTANT !!
		// The `Parent` key *should not* be set for Root Nodes as their parent
		// *is not part of the scene!*
		// 04/09/2024
		if (objectProps[object].find("Parent") == objectProps[object].end())
			objects.push_back(object);

		for (auto& objectProp : objectProps[object])
		{
			const std::string& propName = objectProp.first;
			const uint32_t sceneRelativeId = objectProp.second;

			auto target = objectsMap.find(sceneRelativeId);

			if (target != objectsMap.end())
			{
				Reflection::GenericValue gv = target->second->ObjectId;
				gv.Type = Reflection::ValueType::GameObject;

				try
				{
					object->SetPropertyValue(propName, gv);
				}
				catch (std::string err)
				{
					std::string valueStr = gv.ToString();

					Log::Warning(std::vformat(
						"Deserialization warning: Failed to set GameObject property of '{}' ({}) to '{}': {}",
						std::make_format_args(object->Name, object->ClassName, valueStr, err)
					));
				}
			}
			else
			{
				Log::Warning(std::vformat(
					"Deserialization warning: {} '{}' refers to invalid scene-relative Object ID {} for prop {}. To avoid UB, it will be NULL'd.",
					std::make_format_args(
						object->ClassName,
						object->Name,
						sceneRelativeId,
						propName
					)
				));

				object->SetPropertyValue(propName, ((GameObject*)nullptr)->ToGenericValue());
			}
		}
	}

	return objects;
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

	float version = getVersion(Contents);
	Log::Info(std::vformat("Scene version is {}", std::make_format_args(version)));

	size_t jsonStartLoc = Contents.find("{");
	std::string jsonFileContents = Contents.substr(jsonStartLoc);

	std::vector<GameObject*> objects{};

	if (version == 1.f)
		objects = LoadMapVersion1(jsonFileContents, SuccessPtr);
	else
		if (version == 2.f)
			objects = LoadMapVersion2(jsonFileContents, SuccessPtr);

	Log::Info("Scene loaded");

	return objects;
}

static nlohmann::json serializeObject(GameObject* Object, bool IsRootNode = false)
{
	nlohmann::json item{};

	for (auto& prop : Object->GetProperties())
	{
		const std::string& propName = prop.first;
		const Reflection::Property& propInfo = prop.second;

		if (!propInfo.Set && propName != "ObjectId" && propName != "Class")
			continue;

		// !! IMPORTANT !!
		// The `Parent` key *should not* be set for Root Nodes as their parent
		// *is not part of the scene!*
		// 04/09/2024
		if (IsRootNode && propName == "Parent")
			continue;

		std::string serializedAs = propName;

		auto specialNamesIt = SpecialPropertyToSerializedName.find(propName);

		if (specialNamesIt != SpecialPropertyToSerializedName.end())
			serializedAs = specialNamesIt->second;

		Reflection::GenericValue value = propInfo.Get(Object);

		switch (prop.second.Type)
		{
		case (Reflection::ValueType::Bool):
		{
			item[serializedAs] = value.AsBool();
			break;
		}
		case (Reflection::ValueType::Integer):
		{
			item[serializedAs] = value.AsInteger();
			break;
		}
		case (Reflection::ValueType::Double):
		{
			item[serializedAs] = value.AsDouble();
			break;
		}
		case (Reflection::ValueType::String):
		{
			item[serializedAs] = value.AsString();
			break;
		}
		
		case (Reflection::ValueType::Color):
		{
			Color col(value);
			item[serializedAs] = { col.R, col.G, col.B };
			break;
		}
		case (Reflection::ValueType::Vector3):
		{
			Vector3 vec(value);
			item[serializedAs] = { vec.X, vec.Y, vec.Z };
			break;
		}
		case (Reflection::ValueType::Matrix):
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

		case (Reflection::ValueType::GameObject):
		{
			auto target = GameObject::FromGenericValue(value);

			if (target)
				item[serializedAs] = value.AsInteger();
			else
				item[serializedAs] = PHX_GAMEOBJECT_NULL_ID;
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

	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::chrono::year_month_day ymd = std::chrono::floor<std::chrono::days>(now);

	std::string dateStr = std::to_string((uint32_t)ymd.day()) + "-"
							+ std::to_string((uint32_t)ymd.month()) + "-"
							+ std::to_string((int32_t)ymd.year()) + "\n";

	std::string contents = std::string("PHNXENGI\n")
							+ "#Version 2.00\n"
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
