#include<datatype/GameObject.hpp>

static unsigned int NumGameObjects = 0;

GameObjectFactory::GameObjectMapType* GameObjectFactory::GameObjectMap = new GameObjectMapType();

GameObject::GameObject()
{
	//this->Parent = nullptr;
}

/*
GameObject::~GameObject()
{
	if (this->Parent)
		this->Parent->RemoveChild(this->GameObjectId);

	// delete all the children
	//for (unsigned int ChildIndex = 0; ChildIndex < this->m_children.size(); ChildIndex++)
	//	this->m_children[ChildIndex]->~GameObject();
}
*/

/*
GameObject* GameObjectFactory::CreateGameObject(std::string ObjectClass)
{
	return GameObjectFactory::CreateGameObject(ObjectClass);
}

GameObject* GameObjectFactory::CreateGameObject(const char* ObjectClass)
{
	std::string ObjClass = *new std::string(ObjectClass);
	return GameObjectFactory::CreateGameObject(ObjClass);
}
*/

std::vector<std::shared_ptr<GameObject>> GameObject::GetChildren()
{
	return this->m_children;
}

std::shared_ptr<GameObject> GameObject::GetChildOfClass(std::string Class)
{
	for (int Index = 0; Index < this->m_children.size(); Index++)
		if (this->m_children[Index]->ClassName == Class) 
			return this->m_children[Index];

	return nullptr;
}

std::shared_ptr<GameObject> GameObject::GetChild(std::string ChildName)
{
	for (int Index = 0; Index < this->m_children.size(); Index++)
		if (this->m_children[Index]->Name == ChildName)
			return this->m_children[Index];

	return nullptr;
}

void GameObject::SetParent(std::shared_ptr<GameObject> NewParent)
{
	if (!this->ParentLocked)
		NewParent->AddChild(std::shared_ptr<GameObject>(this));
	else
		throw(std::string( "Error: Attempted to set parent of a parent-locked instance!"));
	
}

void GameObject::AddChild(std::shared_ptr<GameObject> NewChild)
{ //Use <Instance>.SetParent
	this->m_children.push_back(NewChild);

	std::shared_ptr<GameObject> MeAsInstance(this);

	NewChild->Parent = MeAsInstance;

	this->OnChildAdded.Fire(NewChild);
}

void GameObject::RemoveChild(unsigned int ToRemoveId)
{
	for (unsigned int ChildIdx = 0; ChildIdx < this->m_children.size(); ChildIdx++)
	{
		if (this->m_children[ChildIdx]->GameObjectId == ToRemoveId)
		{
			this->OnChildRemoving.Fire(this->m_children[ChildIdx]);
			this->m_children.erase(this->m_children.begin() + ChildIdx);
			break;
		}
	}
}

void GameObject::Initialize()
{

	this->m_properties =
	{
		{"GameObjectId", &this->GameObjectId}
	};

}

void GameObject::Update(double DeltaTime)
{
}

void* GameObject::GetProperty(std::string Property)
{
	return this->m_properties.find(Property)->second;
}

std::shared_ptr<GameObject> GameObjectFactory::CreateGameObject(std::string const& ObjectClass)
{
	// TODO: use a hashmap?
	GameObjectMapType::iterator it = GetGameObjectMap()->find(ObjectClass);

	if (it == GetGameObjectMap()->end())
		return NULL;

	std::shared_ptr<GameObject> CreatedObject = it->second();
	CreatedObject->GameObjectId = NumGameObjects;
	NumGameObjects++;
	
	return CreatedObject;
}

GameObjectFactory::GameObjectMapType* GameObjectFactory::GetGameObjectMap()
{
	// Copied from StackOverflow post:
	// 
	// never delete'ed. (exist until program termination)
	// because we can't guarantee correct destruction order 
	if (!GameObjectMap)
		GameObjectMap = new GameObjectMapType;

	return GameObjectMap;
}
