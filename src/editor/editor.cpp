#include<glm/gtc/matrix_transform.hpp>
#include<imgui/imgui.h>

#include"editor/editor.hpp"
#include"editor/intersectionlib.hpp"
#include"gameobject/GameObjects.hpp"
#include"UserInput.hpp"
#include"Debug.hpp"

static char* NewObjectClass = (char*)malloc(32);
static const char* ParentString = "[Parent]";
static double InvalidObjectErrTimeRemaining = 0.f;

static GameObject* CurrentUIHierarchyRoot;

static bool prevWantKeyboard = false;

Editor::Editor()
{
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

static void ResetAndAddObjects()
{
	Objects.clear();

	AddChildrenToObjects(GameObject::s_DataModel);
}

void Editor::Init()
{
	//AddChildrenToObjects(Root);

	//GameWorkspace->OnChildAdded.Connect(ResetAndAddObjects);

	CurrentUIHierarchyRoot = GameObject::s_DataModel;

	Debug::Log("Editor init done");
}

void Editor::Update(double DeltaTime, glm::mat4 CameraMatrix)
{
	InvalidObjectErrTimeRemaining -= DeltaTime;

	Vector3 CamPos = Vector3(CameraMatrix[3].x, CameraMatrix[3].y, CameraMatrix[3].z);

	glm::mat4 ForwardMat = glm::translate(CameraMatrix, glm::vec3(0.0f, 0.0f, -50.0f));
	Vector3 RayTargetPos = Vector3(ForwardMat[3].x, ForwardMat[3].y, ForwardMat[3].z);

	Vector3 RayDir = RayTargetPos - CamPos;

	// TODO
	IntersectionLib::IntersectionResult Result = IntersectionLib::Traceline(CamPos, RayDir, Objects);
	
	//if (Result.DidHit)
	//	this->MyCube3D->Matrix = glm::translate(glm::mat4(1.0f), glm::vec3(Result.HitPosition));
}

static bool ImGuiHierarchyItemsGetter(void* Data, int Index, const char** OutText)
{
	//GameObject* Parent = (GameObject*)Data;

	if (Index == 0)
	{
		*OutText = ParentString;
		return true;
	}

	GameObject* SelectedChild = CurrentUIHierarchyRoot->GetChildren()[Index - 1];

	*OutText = SelectedChild->Name.c_str();

	return true;
}

void Editor::RenderUI()
{
	ImGui::Begin("Editor");

	bool wantKeyboard = ImGui::GetIO().WantCaptureKeyboard;

	ImGui::InputText("New obj class", NewObjectClass, 32);
	bool Create = ImGui::Button("Create obj");

	if (InvalidObjectErrTimeRemaining > 0.f)
		ImGui::Text("Invalid GameObject!");

	if (GameObject* rootParent = CurrentUIHierarchyRoot->GetParent())
	{
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

	ImGui::ListBox(
		std::vformat("Children of {}", std::make_format_args(CurrentUIHierarchyRoot->Name)).c_str(),
		&this->hierarchyCurItem,
		&ImGuiHierarchyItemsGetter,
		nullptr,
		-1
	);

	GameObject* selected = CurrentUIHierarchyRoot;

	if (hierarchyCurItem > 0)
		selected = CurrentUIHierarchyRoot->GetChildren()[this->hierarchyCurItem - 1];

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

		Reflection::Reflectable* refl = dynamic_cast<Reflection::Reflectable*>(selected);

		for (auto& propListItem : selected->ApiReflection->GetProperties())
		{
			const char* name = propListItem.first.c_str();
			Reflection::IProperty& prop = propListItem.second;

			Reflection::GenericValue curVal = selected->ApiReflection->GetPropertyValue(name);

			if (!prop.Set || !prop.Settable)
			{
				// no setter (locked property, such as ClassName or ObjectId)
				// 07/07/2024

				if (prop.Settable)
				{
					prop.Settable = false;

					std::string fullname = selected->GetFullName();
					Debug::Log(std::vformat(
						"Object '{}' had property {}, which had no Setter, with Settable set to True. Fixing.",
						std::make_format_args(fullname, name)
					));
				}

				std::string curValStr = curVal.ToString();

				ImGui::Text(std::vformat("{}: {}", std::make_format_args(name, curValStr)).c_str());

				continue;
			}

			Reflection::GenericValue newVal;

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

				ImGui::InputText(name, buf, 64);

				curVal.String = std::string(buf);
				newVal = curVal;

				free(buf);

				break;
			}

			case (Reflection::ValueType::Bool):
			{
				ImGui::Checkbox(name, &curVal.Bool);
				newVal = curVal;
				
				break;
			}

			case (Reflection::ValueType::Double):
			{
				ImGui::InputDouble(name, &curVal.Double);
				newVal = curVal;

				break;
			}

			case (Reflection::ValueType::Integer):
			{
				ImGui::InputInt(name, &curVal.Integer);
				newVal = curVal;

				break;
			}

			case (Reflection::ValueType::Color):
			{
				Color col = curVal;

				float entry[3] = { col.R, col.G, col.B };

				ImGui::InputFloat3(name, entry);

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

				ImGui::InputFloat3(name, entry);

				vec.X = entry[0];
				vec.Y = entry[1];
				vec.Z = entry[2];

				newVal = vec.ToGenericValue();

				break;
			}

			}

			selected->ApiReflection->SetPropertyValue(name, newVal);
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
			{
				newObj->SetParent(CurrentUIHierarchyRoot);
			}
		}
	}
}
