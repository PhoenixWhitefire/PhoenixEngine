#include"editor/editor.hpp"
#include"BaseMeshes.hpp"
#include"glm/gtc/matrix_transform.hpp"
#include"render/Material.hpp"
#include"gameobject/Model.hpp"
#include <gameobject/Light.hpp>

static char* NewObjectClass = (char*)malloc(32);
static std::vector<std::shared_ptr<GameObject>> EditorCreatedObjs;

Editor::Editor()
{
	printf("created editor object\n");
}

static std::vector<IntersectionLib::HittableObject*> Objects;

void AddChildrenToObjects(std::shared_ptr<GameObject> Parent)
{
	for (std::shared_ptr<GameObject> Obj : Parent->GetChildren())
	{
		std::shared_ptr<Object_Mesh> Obj3D = std::dynamic_pointer_cast<Object_Mesh>(Obj);

		if (Obj3D)
		{
			printf("Handling insertion of %s '%s'\n", Obj->ClassName.c_str(), Obj->Name.c_str());

			IntersectionLib::HittableObject* NewObject = new IntersectionLib::HittableObject();
			NewObject->CollisionMesh = Obj3D->GetRenderMesh();
			NewObject->Id = Objects.size();

			glm::mat4 ModelMatrix = glm::mat4(1.0f);

			std::shared_ptr<Object_Model> ParentModel = std::dynamic_pointer_cast<Object_Model>(Obj->Parent);

			if (ParentModel)
				ModelMatrix = ParentModel->Matrix;

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

void ResetAndAddObjects(std::shared_ptr<GameObject> New)
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

	Mesh* CubeMesh = this->MyCube3D->GetRenderMesh();
	Mesh CubeMeshColored = Mesh(CubeMesh->Vertices, CubeMesh->Indices, glm::vec3(0.0f, 0.0f, 1.0f));

	this->MyCube3D->SetRenderMesh(CubeMeshColored);

	this->Workspace = GameWorkspace;
	this->MyCube->SetParent(this->Workspace);

	//GameWorkspace->OnChildAdded.Connect(ResetAndAddObjects);

	printf("editor init done\n");
}

void Editor::Update(double DeltaTime, glm::mat4 CameraMatrix)
{
	Vector3 CamPos = Vector3(CameraMatrix[3].x, CameraMatrix[3].y, CameraMatrix[3].z);

	glm::mat4 ForwardMat = glm::translate(CameraMatrix, glm::vec3(0.0f, 0.0f, -50.0f));
	Vector3 RayTargetPos = Vector3(ForwardMat[3].x, ForwardMat[3].y, ForwardMat[3].z);

	Vector3 RayDir = RayTargetPos - CamPos;

	// TODO
	//IntersectionLib::IntersectionResult Result = IntersectionLib::Traceline(CamPos, RayDir, Objects);
	
	//if (Result.DidHit)
	//	this->MyCube3D->Matrix = glm::translate(glm::mat4(1.0f), glm::vec3(Result.HitPosition));
}

bool ImGuiHierarchyItemsGetter(void* Data, int Index, const char** OutText)
{
	GameObject* Parent = (GameObject*)Data;

	std::shared_ptr<GameObject> SelectedChild = Parent->GetChildren()[Index];

	*OutText = SelectedChild->Name.c_str();

	return true;
}

void Editor::RenderUI()
{
	ImGui::Begin("Editor");

	ImGui::InputText("New obj class", NewObjectClass, 32);
	bool Create = ImGui::Button("Create obj");

	ImGui::ListBox(
		"Scene hierarchy",
		&this->hierarchyCurItem,
		&ImGuiHierarchyItemsGetter,
		&*this->Workspace,
		this->Workspace->GetChildren().size()
	);

	ImGui::Text("Properties:");

	const std::string ClassFmt = "Class: {}";

	std::shared_ptr<GameObject> selected = this->Workspace->GetChildren()[this->hierarchyCurItem];

	ImGui::Text(std::vformat(ClassFmt, std::make_format_args(selected->ClassName)).c_str());

	for (PropListItem_t propListItem : selected->GetProperties())
	{
		const char* name = std::get<0>(propListItem).c_str();

		PropDef_t prop = std::get<1>(propListItem);
		PropGetSet_t getSet = std::get<1>(prop);

		switch (std::get<0>(prop))
		{

		case (PropType::String):
		{
			auto gt = std::get<0>(getSet)();

			uint8_t allocSize = uint8_t(fmax(64, gt.String.length()));

			char* buf = (char*)malloc(allocSize);

			if (buf == 0)
			{
				throw(std::string("editor.cpp: Text entry buffer was NULL (allocation error)."));
				return;
			}

			memcpy(buf, gt.String.c_str(), allocSize);

			ImGui::InputText(name, buf, 64);

			gt.String = buf;

			std::get<1>(getSet)(gt);

			free(buf);

			break;
		}

		case (PropType::Bool):
		{
			auto gt = std::get<0>(getSet)();

			ImGui::Checkbox(name, &gt.Bool);

			std::get<1>(getSet)(gt);
			break;
		}

		case (PropType::Double):
		{
			auto gt = std::get<0>(getSet)();

			ImGui::InputDouble(name, &gt.Double);

			std::get<1>(getSet)(gt);
			break;
		}

		case (PropType::Integer):
		{
			auto gt = std::get<0>(getSet)();

			ImGui::InputInt(name, &gt.Integer);

			std::get<1>(getSet)(gt);
			break;
		}

		case (PropType::Color):
		{
			auto gt = std::get<0>(getSet)();
			float entry[3] = {gt.Color3.R, gt.Color3.G, gt.Color3.B};

			ImGui::InputFloat3(name, entry);

			gt.Color3.R = entry[0];
			gt.Color3.G = entry[1];
			gt.Color3.B = entry[2];

			std::get<1>(getSet)(gt);
			break;
		}

		case (PropType::Vector3):
		{
			auto gt = std::get<0>(getSet)();
			float entry[3] = { gt.Vector3.X, gt.Vector3.Y, gt.Vector3.Z };

			ImGui::InputFloat3(name, entry);

			gt.Vector3.X = entry[0];
			gt.Vector3.Y = entry[1];
			gt.Vector3.Z = entry[2];

			std::get<1>(getSet)(gt);
			break;
		}

		}
	}

	ImGui::End();

	if (Create)
	{
		auto newObj = GameObjectFactory::CreateGameObject(NewObjectClass);
		newObj->SetParent(this->Workspace);

		// so that silly little C++ STL doesnt de-alloc our new object
		// and instantly crash
		// (this whole codebase is garbage and i have no clue how to fix it)
		// 13/06/2024
		EditorCreatedObjs.push_back(newObj);
	}
}
