#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtx/euler_angles.hpp>
#include<imgui/imgui.h>

#include"Editor.hpp"
#include"gameobject/GameObjects.hpp"
#include"asset/TextureManager.hpp"
#include"UserInput.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

constexpr uint32_t OBJECT_NEW_CLASSNAME_BUFSIZE = 16;
constexpr uint32_t MATERIAL_NEW_NAME_BUFSIZE = 16;
constexpr uint32_t MATERIAL_TEXTUREPATH_BUFSIZE = 64;
constexpr const char* MATERIAL_NEW_NAME_DEFAULT = "newmaterial";

static const char* ParentString = "[Parent]";

static std::string ErrorTexture = "textures/MISSING2_MaximumADHD_status_1665776378145304579.png";

static nlohmann::json DefaultNewMaterial{};

static char* bufferInitialize(uint32_t capacity, const std::string& initializeValue = "")
{
	char* buf = (char*)malloc(capacity);

	if (!buf)
		throw("There are bigger problems at hand.");

	if (initializeValue != "")
		for (uint32_t i = 0; i < capacity; i++)
			buf[i] = 0;
	else
		for (uint32_t i = 0; i < capacity; i++)
			buf[i] = i < initializeValue.size() ? initializeValue.at(i) : 0;

	return buf;
}

Editor::Editor()
{
	m_NewObjectClass = bufferInitialize(OBJECT_NEW_CLASSNAME_BUFSIZE);
	m_MtlCreateNameBuf = bufferInitialize(MATERIAL_NEW_NAME_BUFSIZE, MATERIAL_NEW_NAME_DEFAULT);
	m_MtlDiffuseBuf = bufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlSpecBuf = bufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);

	DefaultNewMaterial["albedo"] = "textures/plastic.png";
}

static void AddChildrenToObjects(GameObject* Parent)
{
	for (GameObject* Obj : Parent->GetChildren())
	{
		Object_Base3D* Obj3D = dynamic_cast<Object_Base3D*>(Obj);

		if (Obj3D)
		{
			/*IntersectionLib::HittableObject* NewObject = new IntersectionLib::HittableObject();
			NewObject->CollisionMesh = Obj3D->GetRenderMesh();
			NewObject->Id = Objects.size();

			glm::mat4 ModelMatrix = glm::mat4(1.0f);

			Object_Model* ParentModel = dynamic_cast<Object_Model*>(Obj->Parent);*/

			//if (ParentModel)
			//	ModelMatrix = ParentModel->Matrix;

			//NewObject->Matrix = ModelMatrix * Obj3D->Matrix;
			
			//glm::mat4 Scale = glm::mat4(1.0f);
			//Scale = glm::scale(Scale, (glm::vec3)Obj3D->Size);

			//NewObject->Matrix = NewObject->Matrix * Scale;

			//Objects.push_back(NewObject);
		}

		if (Obj->GetChildren().size() > 0)
			AddChildrenToObjects(Obj);
	}
}

//static void ResetAndAddObjects()
//{
//	Objects.clear();
//
//	AddChildrenToObjects(GameObject::s_DataModel);
//}

void Editor::Init()
{
	//AddChildrenToObjects(Root);

	//GameWorkspace->OnChildAdded.Connect(ResetAndAddObjects);

	m_CurrentUIHierarchyRoot = GameObject::s_DataModel;

	Debug::Log("Editor init done");
}

void Editor::Update(double DeltaTime, glm::mat4 CameraTransform)
{
	m_InvalidObjectErrTimeRemaining -= DeltaTime;

	Vector3 CamPos = Vector3(CameraTransform[3].x, CameraTransform[3].y, CameraTransform[3].z);

	glm::mat4 ForwardMat = glm::translate(CameraTransform, glm::vec3(0.0f, 0.0f, -50.0f));
	Vector3 RayTargetPos = Vector3(ForwardMat[3].x, ForwardMat[3].y, ForwardMat[3].z);

	Vector3 RayDir = RayTargetPos - CamPos;

	//if (Result.DidHit)
	//	this->MyCube3D->Matrix = glm::translate(glm::mat4(1.0f), glm::vec3(Result.HitPosition));
}

static std::vector<GameObject*> getVisibleChildren(GameObject** CurrentUIHierarchyRoot)
{
	std::vector<GameObject*> children;

	if (*CurrentUIHierarchyRoot)
		if (GameObject::s_WorldArray.find((*CurrentUIHierarchyRoot)->ObjectId) == GameObject::s_WorldArray.end())
			*CurrentUIHierarchyRoot = nullptr;

	if (*CurrentUIHierarchyRoot)
		children = (*CurrentUIHierarchyRoot)->GetChildren();
	else
		for (auto& it : GameObject::s_WorldArray)
		{
			if (!it.second->GetParent())
				children.push_back(it.second);
		}
			
	return children;
}

static bool objectsIterator(void* Root, int Index, const char** OutText)
{
	//GameObject* Parent = (GameObject*)Data;

	size_t negativeOffset = 1;

	if (Index == 0)
	{
		if (Root)
		{
			*OutText = ParentString;
			return true;
		}
		else
			negativeOffset = 0;
	}

	GameObject* selected = getVisibleChildren((GameObject**)Root).at((size_t)Index - negativeOffset);
	*OutText = selected->Name.c_str();

	return true;
}

static bool mtlIterator(void*, int index, const char** outText)
{
	RenderMaterial* selected = RenderMaterial::GetLoadedMaterials()[index];

	*outText = selected->Name.c_str();

	return true;
}

// 02/09/2024
// That one Gianni Matragrano shitpost where it's a
// baguette with a doge face on a white background low-res
// with the caption "pain"
// and it's just him screaming into the mic
void Editor::m_RenderMaterialEditor()
{
	ImGui::Begin("Materials");

	ImGui::InputText("New material", m_MtlCreateNameBuf, MATERIAL_NEW_NAME_BUFSIZE);

	if (ImGui::Button("Create"))
	{
		FileRW::WriteFile(
			"materials/" + std::string(m_MtlCreateNameBuf) + ".mtl",
			DefaultNewMaterial.dump(2), true
		);

		RenderMaterial::GetMaterial(m_MtlCreateNameBuf);
	}

	ImGui::ListBox(
		"Active materials",
		&m_MtlCurItem,
		&mtlIterator,
		nullptr,
		static_cast<int>(RenderMaterial::GetLoadedMaterials().size())
	);

	if (m_MtlCurItem == -1)
	{
		ImGui::End();

		return;
	}

	RenderMaterial* curItem = RenderMaterial::GetLoadedMaterials().at(m_MtlCurItem);

	TextureManager* texManager = TextureManager::Get();

	Texture* colorMap = texManager->GetTextureResource(curItem->ColorMap);
	Texture* metallicRoughnessMap = texManager->GetTextureResource(curItem->MetallicRoughnessMap);

	if (m_MtlCurItem != m_MtlPrevItem)
	{
		for (uint32_t i = 0; i < MATERIAL_TEXTUREPATH_BUFSIZE; i++)
			m_MtlDiffuseBuf[i] = i < colorMap->ImagePath.size()
									? colorMap->ImagePath.at(i)
									: 0;

		if (metallicRoughnessMap)
			for (uint32_t i = 0; i < MATERIAL_TEXTUREPATH_BUFSIZE; i++)
				m_MtlSpecBuf[i] = i < metallicRoughnessMap->ImagePath.size()
									? metallicRoughnessMap->ImagePath.at(i)
									: 0;
		else
			for (int i = 0; i < 64; i++)
				m_MtlSpecBuf[i] = 0;
	}

	m_MtlPrevItem = m_MtlCurItem;

	ImGui::InputText("Diffuse", m_MtlDiffuseBuf, 64);
	
	ImGui::Image(
		// first cast to uint64_t to get rid of the
		// "'type cast': conversion from 'uint32_t' to 'void *' of greater size"
		// warning
		(void*)((uint64_t)colorMap->GpuId),
		ImVec2(256, 256),
		// Flip the Y axis. Either OpenGL or Dear ImGui is bottom-up
		ImVec2(0, 1),
		ImVec2(1, 0)
	);

	ImGui::Text(std::vformat(
		"Resolution: {}x{}",
		std::make_format_args(colorMap->Width, colorMap->Height)
	).c_str());

	ImGui::Text(std::vformat(
		"# Color channels: {}",
		std::make_format_args(colorMap->NumColorChannels)
	).c_str());

	ImGui::InputText("Specular", m_MtlSpecBuf, 64);

	if (metallicRoughnessMap)
	{
		ImGui::Image(
			// first cast to uint64_t to get rid of the
			// "'type cast': conversion from 'uint32_t' to 'void *' of greater size"
			// warning
			(void*)((uint64_t)metallicRoughnessMap->GpuId),
			ImVec2(256, 256),
			// Flip the Y axis. Either OpenGL or Dear ImGui is bottom-up
			ImVec2(0, 1),
			ImVec2(1, 0)
		);

		ImGui::Text(std::vformat(
			"Resolution: {}x{}",
			std::make_format_args(metallicRoughnessMap->Width, metallicRoughnessMap->Height)
		).c_str());

		ImGui::Text(std::vformat(
			"# Color channels: {}",
			std::make_format_args(metallicRoughnessMap->NumColorChannels)
		).c_str());
	}

	if (ImGui::Button("Update textures"))
	{
		curItem->ColorMap = texManager->LoadTextureFromPath(m_MtlDiffuseBuf);

		if (strlen(m_MtlSpecBuf) > 0)
			curItem->MetallicRoughnessMap = texManager->LoadTextureFromPath(m_MtlSpecBuf);
	}

	ImGui::Checkbox("Has translucency", &curItem->HasTranslucency);
	ImGui::InputFloat("Spec pow", &curItem->SpecExponent);
	ImGui::InputFloat("Spec mul", &curItem->SpecMultiply);

	if (ImGui::Button("Save"))
	{
		nlohmann::json newMtlConfig{};

		newMtlConfig["albedo"] = colorMap->ImagePath;

		if (metallicRoughnessMap)
			newMtlConfig["specular"] = metallicRoughnessMap->ImagePath;

		newMtlConfig["specExponent"] = curItem->SpecExponent;
		newMtlConfig["specMultiply"] = curItem->SpecMultiply;
		newMtlConfig["translucency"] = curItem->HasTranslucency;

		// overwrite the model material override
		auto start = curItem->Name.find("models/");
		if (start != std::string::npos)
			FileRW::WriteFile(curItem->Name + "/material.mtl", newMtlConfig.dump(2), true);

		FileRW::WriteFile(
			"materials/" + curItem->Name + ".mtl",
			newMtlConfig.dump(2),
			true
		);
	}

	ImGui::End();
}

void Editor::RenderUI()
{
	m_RenderMaterialEditor();

	ImGui::Begin("Editor");

	ImGui::InputText("New object", m_NewObjectClass, 32);
	bool createObject = ImGui::Button("Create");

	if (m_InvalidObjectErrTimeRemaining > 0.f)
		ImGui::Text("Invalid GameObject!");

	if (m_CurrentUIHierarchyRoot)
	{
		bool ascendHierarchy = ImGui::Button(std::vformat(
			"Ascend to parent {}",
			std::make_format_args(m_CurrentUIHierarchyRoot->Name)
		).c_str());

		if (ascendHierarchy)
		{
			GameObject* prevRoot = m_CurrentUIHierarchyRoot;
			m_CurrentUIHierarchyRoot = m_CurrentUIHierarchyRoot->GetParent();

			std::vector<GameObject*> newChildren = getVisibleChildren(&m_CurrentUIHierarchyRoot);

			auto prevRootInCurRootIt = std::find(newChildren.begin(), newChildren.end(), prevRoot);
			// try to select the object that we were just inside
			if (prevRootInCurRootIt != newChildren.end())
				m_HierarchyCurItem = static_cast<int>(prevRootInCurRootIt - newChildren.begin() + 1);
		}
	}

	std::vector<GameObject*> children = getVisibleChildren(&m_CurrentUIHierarchyRoot);

	std::string parentText;

	if (m_CurrentUIHierarchyRoot)
	{
		parentText = std::vformat(
			"Children of {}",
			std::make_format_args(m_CurrentUIHierarchyRoot->Name)
		).c_str();
	}
	else
		parentText = "Root objects";

	ImGui::ListBox(
		parentText.c_str(),
		&m_HierarchyCurItem,
		&objectsIterator,
		&m_CurrentUIHierarchyRoot,
		static_cast<int>(children.size()) + (m_CurrentUIHierarchyRoot ? 1 : 0)
	);

	GameObject* selected = nullptr;

	int32_t startOffset = m_CurrentUIHierarchyRoot ? 1 : 0;

	if (m_HierarchyCurItem > startOffset - 1)
		if (children.size() > m_HierarchyCurItem - startOffset)
			selected = children[(size_t)m_HierarchyCurItem - startOffset];
		else
			m_HierarchyCurItem = -1;

	else if (m_HierarchyCurItem == 0) // index 0 is the `[Parent]` item
		selected = m_CurrentUIHierarchyRoot;

	if (selected)
	{
		size_t numChildren = selected->GetChildren().size();

		bool stepInto = false;
		
		if (numChildren > 0)
			stepInto = ImGui::Button(std::vformat(
				"View {} children",
				std::make_format_args(numChildren)
			).c_str());
		else
			stepInto = ImGui::Button("Step into");

		if (stepInto)
		{
			m_CurrentUIHierarchyRoot = selected;
			m_HierarchyCurItem = -1;
		}
	}

	// can't delete datamodel
	if (selected != GameObject::s_DataModel)
		if (ImGui::Button("Destroy"))
		{
			if (selected && selected == m_CurrentUIHierarchyRoot && m_CurrentUIHierarchyRoot->GetParent())
				m_CurrentUIHierarchyRoot = m_CurrentUIHierarchyRoot->GetParent();

			delete selected;
			selected = nullptr;

			m_HierarchyCurItem = -1;
		}

	if (selected)
	{
		Object_Script* script = dynamic_cast<Object_Script*>(selected);

		if (script)
			if (ImGui::Button("Reload"))
				script->Reload();

		ImGui::Text("Properties:");

		auto& props = selected->GetProperties();

		for (auto& propListItem : props)
		{
			const char* propName = propListItem.first.c_str();
			const IProperty& prop = propListItem.second;

			const Reflection::GenericValue curVal = selected->GetPropertyValue(propName);

			if (!prop.Set)
			{
				// no setter (locked property, such as ClassName or ObjectId)
				// 07/07/2024

				std::string curValStr = curVal.ToString();

				ImGui::Text(std::vformat("{}: {}", std::make_format_args(propName, curValStr)).c_str());

				continue;
			}

			Reflection::GenericValue newVal = curVal;

			switch (curVal.Type)
			{

			case (Reflection::ValueType::String):
			{
				std::string str = curVal.AsString();

				uint8_t allocSize = uint8_t(fmax(64, str.length()));

				char* buf = (char*)malloc(allocSize);

				if (buf == 0)
				{
					throw("editor.cpp: Text entry buffer was NULL (allocation error).");
					return;
				}

				memcpy(buf, str.c_str(), allocSize);

				ImGui::InputText(propName, buf, 64);

				str = std::string(buf);

				free(buf);

				break;
			}

			case (Reflection::ValueType::Bool):
			{
				bool b = newVal.AsBool();

				ImGui::Checkbox(propName, &b);
				newVal = b;

				break;
			}

			case (Reflection::ValueType::Double):
			{
				double d = newVal.AsDouble();

				ImGui::InputDouble(propName, &d);
				newVal = d;

				break;
			}

			case (Reflection::ValueType::Integer):
			{
				// TODO BIG BAD HACK HACK HACK
				// stoobid Dear ImGui :'(
				// only allows 32-bit integer input
				// 01/09/2024
				int32_t valAs32Bit = static_cast<int32_t>(curVal.AsInteger());

				ImGui::InputInt(propName, &valAs32Bit);

				newVal = valAs32Bit;

				break;
			}

			case (Reflection::ValueType::GameObject):
			{
				// TODO BIG BAD HACK HACK HACK
				// stoobid Dear ImGui :'(
				// only allows 32-bit integer input
				// 01/09/2024
				int32_t id = static_cast<int32_t>(curVal.AsInteger());

				ImGui::InputInt(propName, &id);

				newVal = id;

				break;
			}

			case (Reflection::ValueType::Color):
			{
				Color col = curVal;

				float entry[3] = { col.R, col.G, col.B };

				ImGui::ColorEdit3(propName, entry, ImGuiColorEditFlags_None);

				//ImGui::InputFloat3(propName, entry);

				col.R = entry[0];
				col.G = entry[1];
				col.B = entry[2];

				newVal = col.ToGenericValue();

				break;
			}

			case (Reflection::ValueType::Vector3):
			{
				Vector3 vec = curVal;

				float entry[3] =
				{
					static_cast<float>(vec.X),
					static_cast<float>(vec.Y),
					static_cast<float>(vec.Z)
				};

				ImGui::InputFloat3(propName, entry);

				vec.X = entry[0];
				vec.Y = entry[1];
				vec.Z = entry[2];

				newVal = vec.ToGenericValue();

				break;
			}

			case (Reflection::ValueType::Matrix):
			{
				glm::mat4 mat = curVal.AsMatrix();

				ImGui::Text("Transform:");

				float pos[3] =
				{
					mat[3][0],
					mat[3][1],
					mat[3][2]
				};

				// PLEASE GOD JUST WORK ALREADY
				// 21/09/2024
				glm::vec3 rotrads{};
				
				glm::extractEulerAngleXYZ(mat, rotrads.x, rotrads.y, rotrads.z);

				//mat = glm::rotate(mat, -rotrads[0], glm::vec3(1.f, 0.f, 0.f));
				//mat = glm::rotate(mat, -rotrads[1], glm::vec3(0.f, 1.f, 0.f));
				//mat = glm::rotate(mat, -rotrads[2], glm::vec3(0.f, 0.f, 1.f));
				
				float rotdegs[3] =
				{
					glm::degrees(rotrads.x),
					glm::degrees(rotrads.y),
					glm::degrees(rotrads.z)
				};

				ImGui::InputFloat3("Position", pos);
				ImGui::InputFloat3("Rotation", rotdegs);

				mat = glm::mat4(1.f);

				mat[3][0] = pos[0];
				mat[3][1] = pos[1];
				mat[3][2] = pos[2];

				mat *= glm::eulerAngleXYZ(glm::radians(rotdegs[0]), glm::radians(rotdegs[1]), glm::radians(rotdegs[2]));

				newVal = Reflection::GenericValue(mat);

				break;
			}

			default:
			{
				int typeId = static_cast<int>(curVal.Type);
				const std::string& typeName = Reflection::TypeAsString(curVal.Type);

				ImGui::Text(std::vformat(
					"{}: <Display of ID:{} ('{}') types not unavailable>",
					std::make_format_args(propName, typeId, typeName)
				).c_str());

				break;
			}

			}

			selected->SetPropertyValue(propName, newVal);
		}
	}

	ImGui::End();

	if (createObject)
	{
		if (!GameObject::IsValidObjectClass(m_NewObjectClass))
			m_InvalidObjectErrTimeRemaining = 2.f;
		else
		{
			GameObject* newObj = GameObject::Create(m_NewObjectClass);

			if (m_CurrentUIHierarchyRoot)
				newObj->SetParent(m_CurrentUIHierarchyRoot);
		}
	}
}
