#include<datatype/GameObject.hpp>
#include<iterator>
#include<algorithm>

static unsigned int NumGameObjects = 0;

GameObjectFactory::GameObjectMapType* GameObjectFactory::GameObjectMap = new GameObjectMapType();

// https://stackoverflow.com/questions/7571937/how-to-delete-items-from-a-stdvector-given-a-list-of-indices
template<typename T>
inline std::vector<T> erase_indices(const std::vector<T>& data, std::vector<size_t>& indicesToDelete/* can't assume copy elision, don't pass-by-value */)
{
	if (indicesToDelete.empty())
		return data;

	std::vector<T> ret;
	ret.reserve(data.size() - indicesToDelete.size());

	std::sort(indicesToDelete.begin(), indicesToDelete.end());

	// new we can assume there is at least 1 element to delete. copy blocks at a time.
	typename std::vector<T>::const_iterator itBlockBegin = data.begin();
	for (std::vector<size_t>::const_iterator it = indicesToDelete.begin(); it != indicesToDelete.end(); ++it)
	{
		typename std::vector<T>::const_iterator itBlockEnd = data.begin() + *it;
		if (itBlockBegin != itBlockEnd)
		{
			std::copy(itBlockBegin, itBlockEnd, std::back_inserter(ret));
		}
		itBlockBegin = itBlockEnd + 1;
	}

	// copy last block.
	if (itBlockBegin != data.end())
	{
		std::copy(itBlockBegin, data.end(), std::back_inserter(ret));
	}

	return ret;
}

GameObject::GameObject()
{
	//this->Parent = nullptr;

	m_properties.insert(std::pair(
		std::string("ClassName"),
		std::pair(PropType::String, std::pair(
			[this]() { return this->GetClassName(); },
			[this](GenericType gt) { this->SetName(gt.String); }
		))
	));
	m_properties.insert(std::pair(
		std::string("Name"),
		std::pair(PropType::String, std::pair(
			[this]() { return this->GetName(); },
			[this](GenericType gt) { this->SetName(gt.String); }
		))
	));
	m_properties.insert(std::pair(
		std::string("Enabled"),
		std::pair(PropType::Bool, std::pair(
			[this]() { return this->GetEnabled(); },
			[this](GenericType gt) { this->SetEnabled(gt.Bool); }
		))
	));
}

GameObject::~GameObject()
{
	if (this->Parent)
		this->Parent->RemoveChild(this->GameObjectId);

	// delete all the children
	//for (unsigned int ChildIndex = 0; ChildIndex < this->m_children.size(); ChildIndex++)
	//	this->m_children[ChildIndex]->~GameObject();

	this->m_children.clear();
}


void GameObject::Initialize()
{
	
}

void GameObject::Update(double DeltaTime)
{
}

PropList_t GameObject::GetProperties()
{
	return this->m_properties;
}

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
			//this->OnChildRemoving.Fire(this->m_children[ChildIdx]);

			//std::vector<size_t> toDelete = { ChildIdx };

			//this->m_children = erase_indices<std::shared_ptr<GameObject>>(m_children, toDelete);

			//this->m_children.insert(ChildIdx, nullptr);
			break;
		}
	}
}

GenericType GameObject::GetClassName()
{
	return
	{
		PropType::String,
		this->ClassName
	};
}

GenericType GameObject::GetName()
{
	return
	{
		PropType::String,
		this->Name
	};
}

GenericType GameObject::GetEnabled()
{
	return
	{
		PropType::Bool,
		"",
		this->Enabled
	};
}

void GameObject::SetName(std::string name)
{
	this->Name = name;
}

void GameObject::SetEnabled(bool enabled)
{
	this->Enabled = enabled;
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

	CreatedObject->ClassName = ObjectClass;
	CreatedObject->Name = ObjectClass;

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
