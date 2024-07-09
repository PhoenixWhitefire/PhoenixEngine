#include<glm/gtc/matrix_transform.hpp>
#include<imgui/imgui.h>

#include"editor/editor.hpp"
#include"editor/intersectionlib.hpp"
#include"gameobject/GameObjects.hpp"
#include"Debug.hpp"

static char* NewObjectClass = (char*)malloc(32);
static std::vector<std::shared_ptr<GameObject>> EditorCreatedObjs;
static const char* ParentString = "[Parent]";
static double InvalidObjectErrTimeRemaining = 0.f;

static std::shared_ptr<GameObject> CurrentUIHierarchyRoot;

Editor::Editor()
{
	Debug::Log("Editor object was created");
}

static std::vector<IntersectionLib::HittableObject*> Objects;

static void AddChildrenToObjects(std::shared_ptr<GameObject> Parent)
{
	for (std::shared_ptr<GameObject> Obj : Parent->GetChildren())
	{
		std::shared_ptr<Object_Base3D> Obj3D = std::dynamic_pointer_cast<Object_Base3D>(Obj);

		if (Obj3D)
		{
			IntersectionLib::HittableObject* NewObject = new IntersectionLib::HittableObject();
			NewObject->CollisionMesh = Obj3D->GetRenderMesh();
			NewObject->Id = Objects.size();

			glm::mat4 ModelMatrix = glm::mat4(1.0f);

			std::shared_ptr<Object_Model> ParentModel = std::dynamic_pointer_cast<Object_Model>(Obj->Parent);

			//if (ParentModel)
			//	ModelMatrix = ParentModel->Matrix;

			NewObject->Matrix = ModelMatrix * Obj3D->Matrix;
			
			glm::mat4 Scale = glm::mat4(1.0f);
			Scale = glm::scale(Scale, (glm::vec3)Obj3D->Size);

			NewObject->Matrix = NewObject->Matrix * Scale;

			Objects.push_back(NewObject);
		}

		if (Obj->GetChildren().size() > 0)
			AddChildrenToObjects(Obj);
	}
}

std::shared_ptr<GameObject> Root;

static void ResetAndAddObjects(std::shared_ptr<GameObject> New)
{
	Objects.clear();

	AddChildrenToObjects(Root);
}

void Editor::Init(std::shared_ptr<GameObject> GameWorkspace)
{
	Root = GameWorkspace;
	//AddChildrenToObjects(Root);

	this->MyCube = GameObjectFactory::CreateGameObject("Primitive");
	this->MyCube3D = std::dynamic_pointer_cast<Object_Primitive>(MyCube);

	this->MyCube->Name = "RaycastTEST";

	this->MyCube3D->ColorRGB = Color(0.f, 0.f, 1.f);

	this->Workspace = GameWorkspace;
	this->MyCube->SetParent(this->Workspace);

	//GameWorkspace->OnChildAdded.Connect(ResetAndAddObjects);

	CurrentUIHierarchyRoot = Workspace;

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
	
	if (Result.DidHit)
		this->MyCube3D->Matrix = glm::translate(glm::mat4(1.0f), glm::vec3(Result.HitPosition));
}

static bool ImGuiHierarchyItemsGetter(void* Data, int Index, const char** OutText)
{
	//GameObject* Parent = (GameObject*)Data;

	if (Index == 0)
	{
		*OutText = ParentString;
		return true;
	}

	std::shared_ptr<GameObject> SelectedChild = CurrentUIHierarchyRoot->GetChildren()[Index - 1];

	*OutText = SelectedChild->Name.c_str();

	return true;
}

void Editor::RenderUI()
{
	ImGui::Begin("Editor");

	ImGui::InputText("New obj class", NewObjectClass, 32);
	bool Create = ImGui::Button("Create obj");

	if (InvalidObjectErrTimeRemaining > 0.f)
		ImGui::Text("Invalid GameObject!");

	if (CurrentUIHierarchyRoot->Parent)
	{
		bool AscendHierarchy = ImGui::Button(std::vformat(
			"Ascend to parent {}",
			std::make_format_args(CurrentUIHierarchyRoot->Parent->Name)
		).c_str());

		if (AscendHierarchy)
		{
			CurrentUIHierarchyRoot = CurrentUIHierarchyRoot->Parent;
			this->hierarchyCurItem = 0;
		}
	}

	ImGui::ListBox(
		std::vformat("Children of {}", std::make_format_args(CurrentUIHierarchyRoot->Name)).c_str(),
		&this->hierarchyCurItem,
		&ImGuiHierarchyItemsGetter,
		&*this->Workspace,
		CurrentUIHierarchyRoot->GetChildren().size() + 1
	);

	std::shared_ptr<GameObject> selected = CurrentUIHierarchyRoot;

	if (hierarchyCurItem > 0)
		selected = CurrentUIHierarchyRoot->GetChildren()[this->hierarchyCurItem - 1];

	if (selected->GetChildren().size() > 0)
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

	std::shared_ptr<Object_Script> script = std::dynamic_pointer_cast<Object_Script>(selected);

	ImGui::Text("Properties:");

	for (auto& proc : selected->GetProcedures())
	{
		bool runProc = ImGui::Button(proc.first.c_str());

		if (runProc)
			proc.second();
	}

	for (auto& propListItem : selected->GetProperties())
	{
		PropInfo prop = propListItem.second;

		const char* name = propListItem.first.c_str();

		PropReflection getSet = prop.Reflection;

		auto curVal = getSet.Getter();

		if (!getSet.Setter)
		{
			// no setter (locked property, such as ClassName or ObjectId)
			// 07/07/2024

			std::string curValStr = curVal;

			ImGui::Text(std::vformat("{}: {}", std::make_format_args(name, curValStr)).c_str());

			continue;
		}

		switch (prop.Type)
		{

		case (PropType::String):
		{
			uint8_t allocSize = uint8_t(fmax(64, curVal.String.length()));

			char* buf = (char*)malloc(allocSize);

			if (buf == 0)
			{
				throw(std::string("editor.cpp: Text entry buffer was NULL (allocation error)."));
				return;
			}

			memcpy(buf, curVal.String.c_str(), allocSize);

			ImGui::InputText(name, buf, 64);

			curVal.String = buf;

			getSet.Setter(curVal);

			free(buf);

			break;
		}

		case (PropType::Bool):
		{
			ImGui::Checkbox(name, &curVal.Bool);

			getSet.Setter(curVal);
			break;
		}

		case (PropType::Double):
		{
			ImGui::InputDouble(name, &curVal.Double);
			
			getSet.Setter(curVal);
			break;
		}

		case (PropType::Integer):
		{
			ImGui::InputInt(name, &curVal.Integer);

			getSet.Setter(curVal);
			break;
		}

		case (PropType::Color):
		{
			float entry[3] = {curVal.Color3.R, curVal.Color3.G, curVal.Color3.B};

			ImGui::InputFloat3(name, entry);

			curVal.Color3.R = entry[0];
			curVal.Color3.G = entry[1];
			curVal.Color3.B = entry[2];

			getSet.Setter(curVal);

			break;
		}

		case (PropType::Vector3):
		{
			float entry[3] = { curVal.Vector3.X, curVal.Vector3.Y, curVal.Vector3.Z };

			ImGui::InputFloat3(name, entry);

			curVal.Vector3.X = entry[0];
			curVal.Vector3.Y = entry[1];
			curVal.Vector3.Z = entry[2];

			getSet.Setter(curVal);
			break;
		}

		}
	}

	ImGui::End();

	if (Create)
	{
		auto ValidObjects = GameObjectFactory::GameObjectMap;

		if (ValidObjects->find(NewObjectClass) == ValidObjects->end())
			InvalidObjectErrTimeRemaining = 2.f;
		else
		{
			auto newObj = GameObjectFactory::CreateGameObject(NewObjectClass);
			newObj->SetParent(CurrentUIHierarchyRoot);

			// so that silly little C++ STL doesnt de-alloc our new object
			// and instantly crash
			// (this whole codebase is garbage and i have no clue how to fix it)
			// 13/06/2024
			EditorCreatedObjs.push_back(newObj);
		}
	}
}
