#include"editor/editor.hpp"
#include"BaseMeshes.hpp"
#include"glm/gtc/matrix_transform.hpp"
#include"datatype/Material.hpp"
#include"gameobject/Model.hpp"

static MaterialGetter* CubeMatGetter = new MaterialGetter();

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
	/*printf("NEW OBJECT '%s' of class `%s` ADDED!", New->Name.c_str(), New->ClassName.c_str());

	Objects.clear();

	AddChildrenToObjects(Root);*/
}

void Editor::Init(std::shared_ptr<GameObject> GameWorkspace)
{
	Root = GameWorkspace;
	AddChildrenToObjects(Root);

	this->MyCube = GameObjectFactory::CreateGameObject("Primitive");
	this->MyCube3D = std::dynamic_pointer_cast<Object_Primitive>(MyCube);

	this->MyCube->Name = "RaycastTEST";

	Mesh* CubeMesh = this->MyCube3D->GetRenderMesh();
	Mesh CubeMeshColored = Mesh(CubeMesh->Vertices, CubeMesh->Indices, glm::vec3(0.0f, 0.0f, 1.0f));

	this->MyCube3D->SetRenderMesh(CubeMeshColored);

	Material* CubeMat = CubeMatGetter->GetMaterial("plastic");

	this->Workspace = GameWorkspace;
	this->MyCube->SetParent(this->Workspace);

	GameWorkspace->OnChildAdded.Connect(ResetAndAddObjects);

	printf("editor init done\n");
}

void Editor::Update(double DeltaTime, glm::mat4 CameraMatrix)
{
	Vector3 CamPos = Vector3(CameraMatrix[3].x, CameraMatrix[3].y, CameraMatrix[3].z);

	glm::mat4 ForwardMat = glm::translate(CameraMatrix, glm::vec3(0.0f, 0.0f, -50.0f));
	Vector3 RayTargetPos = Vector3(ForwardMat[3].x, ForwardMat[3].y, ForwardMat[3].z);

	IntersectionLib::IntersectionResult Result = IntersectionLib::Traceline(CamPos, RayTargetPos, Objects);
	
	Vector3 ResultPos = Result.DidHit ? Result.HitPosition : RayTargetPos;

	this->MyCube3D->Matrix = glm::translate(glm::mat4(1.0f), glm::vec3(ResultPos));
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

	ImGui::ListBox(
		"Scene hierarchy",
		&this->hierarchyCurItem,
		&ImGuiHierarchyItemsGetter,
		&*this->Workspace,
		this->Workspace->GetChildren().size()
	);

	ImGui::Text("Properties:");

	std::shared_ptr<GameObject> selected = this->Workspace->GetChildren()[this->hierarchyCurItem];

	std::shared_ptr<Object_Base3D> sel3D = std::dynamic_pointer_cast<Object_Base3D>(selected);

	if (sel3D)
	{
		glm::vec3 pos = glm::vec3(sel3D->Matrix[3]);

		std::string PosStr = "Position: {}, {}, {} ";
		auto fmtArgs = std::make_format_args(pos.x, pos.y, pos.z);

		std::string strFmted = std::vformat(
			PosStr,
			fmtArgs
		);

		ImGui::Text(strFmted.c_str());
	}
	else
	{
		ImGui::Text(":(");
	}

	ImGui::End();
}
