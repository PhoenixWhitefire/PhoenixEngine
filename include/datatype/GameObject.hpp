#pragma once

#define PHX_ASSERT(res, err) if (!res) throw(err)

#include<functional>
#include<vector>
#include<string>
#include<map>

#include"datatype/Vector3.hpp"
#include"datatype/Color.hpp"
#include"datatype/Event.hpp"

enum class PropType
{
	NONE,
	String,
	Bool,
	Double,
	Integer,
	Color,
	Vector3
};

// i just cant 13/06/2024
// 04/08/2024: Removed Vector3 and Color, replaced with void* Pointer
struct GenericType
{
	PropType Type = PropType::Bool;
	std::string String;
	bool Bool = true;
	double Double = 0.f;
	int Integer = 0;
	void* Pointer = nullptr;

	GenericType(std::string str)
	{
		this->Type = PropType::String;
		this->String = str;
	}

	GenericType(bool b)
	{
		this->Type = PropType::Bool;
		this->Bool = b;
	}

	GenericType(double d)
	{
		this->Type = PropType::Double;
		this->Double = d;
	}

	GenericType(int i)
	{
		this->Type = PropType::Integer;
		this->Integer = i;
	}

	GenericType(uint32_t i)
	{
		this->Type = PropType::Integer;
		this->Integer = i;
	}

	GenericType(Vector3 v)
	{
		this->Type = PropType::Vector3;
		this->Pointer = malloc(sizeof(v));
		if (this->Pointer != NULL)
			memcpy(this->Pointer, &v, sizeof(v));
		else
			throw("Tried to cast Vector3 to GenericType, met an allocation error in the process.");
	}

	GenericType(Color v)
	{
		this->Type = PropType::Color;
		this->Pointer = malloc(sizeof(v));
		if (this->Pointer != NULL)
			memcpy(this->Pointer, &v, sizeof(v));
		else
			throw("Tried to cast Vector3 to GenericType, met an allocation error in the process.");
	}

	//~GenericType()
	//{
	//	//if (this->Pointer)
	//		//delete this->Pointer; // 04/08/2024 TODO: Instant crash. Fix. Memory leak because of above malloc.
	//}

	std::string ToString()
	{
		switch (this->Type)
		{
		case (PropType::String):
			return this->String;

		case (PropType::Bool):
			return this->Bool ? "true" : "false";

		case (PropType::Double):
			return std::to_string(this->Double);

		case (PropType::Integer):
			return std::to_string(this->Integer);

		case (PropType::Color):
			return std::string(*(Color*)this->Pointer);

		case (PropType::Vector3):
			return std::string(*(Vector3*)this->Pointer);

		default:
		{
			int tInt = (int)this->Type;
			return std::vformat("Cast failed, Type ID was: {}", std::make_format_args(tInt));
		}
			
		}
	}

	operator std::string()
	{
		return this->Type == PropType::String ? this->String : throw("GenericType was not a String");
	}

	operator bool()
	{
		return this->Type == PropType::Bool ? this->Bool : throw("GenericType was not a Bool");
	}

	operator double()
	{
		return this->Type == PropType::Double ? this->Double : throw("GenericType was not a Double");
	}

	operator int()
	{
		return this->Type == PropType::Integer ? this->Integer : throw("GenericType was not an Integer");
	}

	operator Vector3()
	{
		return this->Type == PropType::Vector3 ? *(Vector3*)this->Pointer : throw("GenericType was not a Vector3");
	}

	operator Color()
	{
		return this->Type == PropType::Color ? *(Color*)this->Pointer : throw("GenericType was not a Color");
	}

	operator std::vector<GenericType>()
	{
		auto vec = std::vector<GenericType>();
		vec.push_back(*this);
	}
};

typedef std::vector<GenericType> GenericTypeArray;

struct PropReflection
{
	std::function<GenericType(void)> Getter;
	std::function<void(GenericType)> Setter;
};

struct PropInfo
{
	PropType Type;
	PropReflection Reflection;
};

typedef std::function<GenericTypeArray(GenericTypeArray)> GenericFunction_t;

typedef std::unordered_map<std::string, PropInfo> PropList_t;

typedef std::unordered_map<std::string, GenericFunction_t> FuncList_t;

class GameObject
{
public:
	GameObject();
	~GameObject();

	std::vector<std::shared_ptr<GameObject>>& GetChildren();
	std::shared_ptr<GameObject> GetChildOfClass(std::string lass);

	void SetParent(std::shared_ptr<GameObject> Parent);
	void AddChild(std::shared_ptr<GameObject> Child);
	void RemoveChild(uint32_t ObjectId);

	virtual void Initialize();
	virtual void Update(double DeltaTime);

	std::shared_ptr<GameObject> GetChild(std::string ChildName);

	PropList_t& GetProperties();
	FuncList_t& GetFunctions();

	PropInfo* GetProperty(std::string Name);
	GenericFunction_t* GetFunction(std::string Name);

	void SetProperty(std::string Name, GenericType gt);
	GenericTypeArray CallFunction(std::string Name, GenericTypeArray args);

	std::string GetFullName();

	static std::shared_ptr<GameObject> DataModel;

	std::string Name = "GameObject";
	std::string ClassName = "GameObject";

	bool DidInit = false;

	std::shared_ptr<GameObject> operator [] (std::string ChildName)
	{
		for (int Index = 0; Index < this->m_children.size(); Index++)
			if (this->m_children[Index]->Name == ChildName)
				return this->m_children[Index];

		return nullptr;
	}

	bool operator ! ()
	{
		return false;
	}

	std::shared_ptr<GameObject> Parent;

	bool Enabled = true;

	Event<std::shared_ptr<GameObject>> OnChildAdded;
	Event<std::shared_ptr<GameObject>> OnChildRemoving;

	bool ParentLocked = false;

	uint32_t ObjectId = 0;

protected:
	std::vector<std::shared_ptr<GameObject>> m_children;

	PropList_t m_properties;
	FuncList_t m_functions;
};

// I followed this StackOverflow post:
// https://stackoverflow.com/a/582456/16875161

struct GameObjectFactory
{
	static std::shared_ptr<GameObject> CreateGameObject(std::string const& ObjectClass);

	// TODO: understand contructor type voodoo, + is this even a good method?
	typedef std::map<std::string, std::shared_ptr<GameObject>(*)()> GameObjectMapType;

	static GameObjectMapType* GetGameObjectMap();

	static GameObjectMapType* GameObjectMap;
};

template<typename T> std::shared_ptr<GameObject> createT_baseGameObject()
{
	return std::shared_ptr<GameObject>(new T);
}

template <typename T>
struct DerivedObjectRegister : GameObjectFactory
{
	DerivedObjectRegister(std::string const& ObjectClass)
	{
		GetGameObjectMap()->insert(std::make_pair(ObjectClass, &createT_baseGameObject<T>));
	}
};
