#include<glm/gtc/matrix_transform.hpp>
#include<imgui/imgui.h>

#include"editor/editor.hpp"
#include"editor/intersectionlib.hpp"
#include"gameobject/GameObjects.hpp"
#include"UserInput.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

static char* NewObjectClass = nullptr;
static const char* ParentString = "[Parent]";
static double InvalidObjectErrTimeRemaining = 0.f;

static GameObject* CurrentUIHierarchyRoot;

static bool prevWantKeyboard = false;

static char* newMtlName = (char*)malloc(64);
static char* diffuseBuf = (char*)malloc(64);
static char* specBuf = (char*)malloc(64);

static std::string DefaultNewMtlName = "new";

static nlohmann::json DefaultNewMaterial{};

Editor::Editor()
{
	NewObjectClass = (char*)malloc(sizeof(char)*32);

	if (!NewObjectClass)
		throw("There are bigger issues at hand");

	for (int i = 0; i < 32; i++)
		NewObjectClass[i] = 0;

	for (int i = 0; i < 64; i++)
		newMtlName[i] = DefaultNewMtlName[i];

	for (int i = 0; i < 64; i++)
	{
		diffuseBuf[i] = 0;
		specBuf[i] = 0;
	}

	DefaultNewMaterial["albedo"] = "textures/plastic.png";

	Debug::Log("Editor object was created");
}

static std::vector<IntersectionLib::HittableObject*> Objects;

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

	CurrentUIHierarchyRoot = GameObject::s_DataModel;

	Debug::Log("Editor init done");
}

void Editor::Update(double DeltaTime, glm::mat4 CameraTransform)
{
	InvalidObjectErrTimeRemaining -= DeltaTime;

	Vector3 CamPos = Vector3(CameraTransform[3].x, CameraTransform[3].y, CameraTransform[3].z);

	glm::mat4 ForwardMat = glm::translate(CameraTransform, glm::vec3(0.0f, 0.0f, -50.0f));
	Vector3 RayTargetPos = Vector3(ForwardMat[3].x, ForwardMat[3].y, ForwardMat[3].z);

	Vector3 RayDir = RayTargetPos - CamPos;

	// TODO
	IntersectionLib::IntersectionResult Result = IntersectionLib::Traceline(CamPos, RayDir, Objects);
	
	//if (Result.DidHit)
	//	this->MyCube3D->Matrix = glm::translate(glm::mat4(1.0f), glm::vec3(Result.HitPosition));
}

static bool ImGuiHierarchyItemsGetter(void*, int Index, const char** OutText)
{
	//GameObject* Parent = (GameObject*)Data;

	if (Index == 0)
	{
		*OutText = ParentString;
		return true;
	}

	GameObject* SelectedChild = CurrentUIHierarchyRoot->GetChildren()[(size_t)Index - 1];

	*OutText = SelectedChild->Name.c_str();

	return true;
}

static bool mtlIterator(void*, int index, const char** outText)
{
	RenderMaterial* selected = RenderMaterial::GetLoadedMaterials()[index];

	*outText = selected->Name.c_str();;

	return true;
}

// 02/09/2024
// That one Gianni Matragrano shitpost where it's a
// baguette with a doge face on a white background low-res
// with the caption "pain"
// and it's just him screaming into the mic
static void materialEditor()
{
	ImGui::Begin("Materials");

	static int mtlCurItem = 0;
	static int prevItem = -1;

	ImGui::InputText("New material", newMtlName, 64);

	if (ImGui::Button("Create"))
	{
		FileRW::WriteFile(
			"materials/" + std::string(newMtlName) + ".mtl",
			DefaultNewMaterial.dump(2), true
		);
		RenderMaterial::GetMaterial(newMtlName);
	}

	ImGui::ListBox(
		"Active materials",
		&mtlCurItem,
		&mtlIterator,
		nullptr,
		static_cast<int>(RenderMaterial::GetLoadedMaterials().size())
	);

	RenderMaterial* curItem = RenderMaterial::GetLoadedMaterials()[mtlCurItem];

	if (mtlCurItem != prevItem)
	{
		for (int i = 0; i < 64; i++)
			diffuseBuf[i] = curItem->DiffuseTextures[0]->ImagePath[i];

		if (curItem->SpecularTextures.size() >= 1)
			for (int i = 0; i < 64; i++)
				specBuf[i] = curItem->SpecularTextures[0]->ImagePath[i];
		else
			for (int i = 0; i < 64; i++)
				specBuf[i] = 0;
	}

	prevItem = mtlCurItem;

	ImGui::InputText("Diffuse", diffuseBuf, 64);
	ImGui::InputText("Specular", specBuf, 64);

	if (ImGui::Button("Update textures"))
	{
		curItem->DiffuseTextures[0] = TextureManager::Get()->LoadTextureFromPath(diffuseBuf);

		if (curItem->HasSpecular)
		{
			if (curItem->SpecularTextures.size() > 0)
				curItem->SpecularTextures[0] = TextureManager::Get()->LoadTextureFromPath(specBuf);
			else
				curItem->SpecularTextures.push_back(TextureManager::Get()->LoadTextureFromPath(specBuf));
		}
	}

	ImGui::InputFloat("Spec pow", &curItem->SpecExponent);
	ImGui::InputFloat("Spec mul", &curItem->SpecMultiply);
	ImGui::Checkbox("Has spec map", &curItem->HasSpecular);

	if (ImGui::Button("Save"))
	{
		nlohmann::json newMtlConfig{};

		newMtlConfig["albedo"] = curItem->DiffuseTextures[0]->ImagePath;

		if (curItem->HasSpecular)
			newMtlConfig["specular"] = curItem->SpecularTextures[0]->ImagePath;

		newMtlConfig["specExponent"] = curItem->SpecExponent;
		newMtlConfig["specMultiply"] = curItem->SpecMultiply;

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
	materialEditor();

	ImGui::Begin("Editor");

	ImGui::InputText("New obj class", NewObjectClass, 32);
	bool Create = ImGui::Button("Create obj");

	if (InvalidObjectErrTimeRemaining > 0.f)
		ImGui::Text("Invalid GameObject!");

	if (GameObject* rootParent = CurrentUIHierarchyRoot->GetParent())
	{
		if (!rootParent)
			throw("WHAT");

		bool AscendHierarchy = ImGui::Button(std::vformat(
			"Ascend to parent {}",
			std::make_format_args(rootParent->Name)
		).c_str());

		if (AscendHierarchy)
		{
			CurrentUIHierarchyRoot = rootParent;
			this->hierarchyCurItem = 0;
		}
	}

	int nItems = static_cast<int>(CurrentUIHierarchyRoot->GetChildren().size() + 1);

	ImGui::ListBox(
		std::vformat("Children of {}", std::make_format_args(CurrentUIHierarchyRoot->Name)).c_str(),
		&this->hierarchyCurItem,
		&ImGuiHierarchyItemsGetter,
		nullptr,
		nItems
	);

	GameObject* selected = CurrentUIHierarchyRoot;

	if (hierarchyCurItem > 0)
		selected = CurrentUIHierarchyRoot->GetChildren()[(size_t)this->hierarchyCurItem - 1];

	if (selected && selected->GetChildren().size() > 0)
	{
		size_t numChildren = selected->GetChildren().size();

		bool ChangeView = ImGui::Button(std::vformat(
			"View {} children",
			std::make_format_args(numChildren)
		).c_str());

		if (ChangeView)
		{
			CurrentUIHierarchyRoot = selected;
			this->hierarchyCurItem = 0;
		}
	}

	// can't delete datamodel
	if (selected != GameObject::s_DataModel)
		if (ImGui::Button("Destroy"))
		{
			if (selected && selected == CurrentUIHierarchyRoot && CurrentUIHierarchyRoot->Parent)
				CurrentUIHierarchyRoot = CurrentUIHierarchyRoot->GetParent();

			delete selected;
			selected = nullptr;
			// in case we delete the bottom-most item
			this->hierarchyCurItem = (hierarchyCurItem - 1) >= 0 ? (hierarchyCurItem - 1) : 0;
		}

	if (selected)
	{
		//Object_Script* script = dynamic_cast<Object_Script*>(selected);

		//if (script)
			//if (ImGui::Button("Reload"))
				//script->Reload();

		ImGui::Text("Properties:");

		auto props = selected->GetProperties();

		for (auto& propListItem : props)
		{
			const char* propName = propListItem.first.c_str();
			IProperty& prop = propListItem.second;

			Reflection::GenericValue curVal = selected->GetPropertyValue(propName);

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
				uint8_t allocSize = uint8_t(fmax(64, curVal.String.length()));

				char* buf = (char*)malloc(allocSize);

				if (buf == 0)
				{
					throw("editor.cpp: Text entry buffer was NULL (allocation error).");
					return;
				}

				memcpy(buf, curVal.String.c_str(), allocSize);

				ImGui::InputText(propName, buf, 64);

				curVal.String = std::string(buf);
				newVal = curVal;

				free(buf);

				break;
			}

			case (Reflection::ValueType::Bool):
			{
				ImGui::Checkbox(propName, &curVal.Bool);
				newVal = curVal;
				
				break;
			}

			case (Reflection::ValueType::Double):
			{
				ImGui::InputDouble(propName, &curVal.Double);
				newVal = curVal;

				break;
			}

			case (Reflection::ValueType::Integer):
			{
				// TODO BIG BAD HACK HACK HACK
				// stoobid imgui :'(
				// only allows 32-bit integer input
				// 01/09/2024
				int32_t valAs32Bit = static_cast<int32_t>(curVal.Integer);

				ImGui::InputInt(propName, &valAs32Bit);

				newVal.Integer = valAs32Bit;
				newVal = curVal;

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

	prevWantKeyboard = ImGui::GetIO().WantCaptureKeyboard;

	ImGui::End();

	if (Create)
	{
		if (!GameObject::IsValidObjectClass(NewObjectClass))
			InvalidObjectErrTimeRemaining = 2.f;
		else
		{
			GameObject* newObj = GameObject::CreateGameObject(NewObjectClass);

			if (CurrentUIHierarchyRoot)
				newObj->SetParent(CurrentUIHierarchyRoot);
		}
	}
}
