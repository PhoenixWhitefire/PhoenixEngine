#include"Reflection.hpp"
#include"datatype/Vector3.hpp"
#include"datatype/Color.hpp"
#include"datatype/GameObject.hpp"

//Reflection::Reflectable* Reflection::Reflectable::ApiReflection = new Reflection::Reflectable();

Reflection::GenericValue::GenericValue()
	: Type(ValueType::Null)
{
}

Reflection::GenericValue::GenericValue(const std::string& str)
	: Type(ValueType::String), String(str)
{
}

Reflection::GenericValue::GenericValue(bool b)
	: Type(ValueType::Bool), Pointer((void*)b)
{
}

Reflection::GenericValue::GenericValue(double d)
	: Type(ValueType::Double), Pointer(*(void**)&d)
{
}

Reflection::GenericValue::GenericValue(int64_t i)
	: Type(ValueType::Integer), Pointer((void*)i)
{
}

Reflection::GenericValue::GenericValue(uint32_t i)
	: Type(ValueType::Integer), Pointer((void*)static_cast<int64_t>(i))
{
}

Reflection::GenericValue::GenericValue(int i)
	: Type(ValueType::Integer), Pointer((void*)static_cast<int64_t>(i))
{
}

Reflection::GenericValue::GenericValue(const glm::mat4& m)
	: Type(ValueType::Matrix), Pointer(malloc(sizeof(m)))
{
	if (!Pointer)
		throw("Allocation error while constructing GenericValue from glm::mat4");

	memcpy(Pointer, &m, sizeof(m));
}

Reflection::GenericValue::GenericValue(const std::vector<GenericValue>& array)
	: Type(ValueType::Array), Array(array)
{
}

Reflection::GenericValue::GenericValue(const std::unordered_map<GenericValue, GenericValue>& map)
	: Type(ValueType::Array)
{
	for (auto& it : map)
	{
		this->Array.push_back(it.first);
		this->Array.push_back(it.second);
	}
}

std::string Reflection::GenericValue::ToString() const
{
	switch (this->Type)
	{
	case (ValueType::Null):
		return "Null";

	case (ValueType::Bool):
		return (bool)this->Pointer ? "true" : "false";

	case (ValueType::Integer):
		return std::to_string((int64_t)this->Pointer);

	case (ValueType::Double):
		return std::to_string(*(double*)&this->Pointer);

	case (ValueType::String):
		return this->String;
	
	case (ValueType::Color):
		return Color(*this).ToString();

	case (ValueType::Vector3):
		return Vector3(*this).ToString();

	case (ValueType::GameObject):
	{
		GameObject* object = GameObject::GetObjectById(*(uint32_t*)&this->Pointer);
		return object ? object->GetFullName() : "NULL GameObject";
	}
	
	case (ValueType::Array):
	{
		if (this->Array.size() > 0)
		{
			uint32_t numTypes = 0;
			std::string typesString = "";

			for (const Reflection::GenericValue& element : this->Array)
			{
				numTypes++;
				if (numTypes > 4)
				{
					typesString += "...|";
					break;
				}
				else
					typesString += Reflection::TypeAsString(element.Type) + "|";
			}

			typesString = typesString.substr(0, typesString.size() - 1);

			return std::vformat("Array<{}>", std::make_format_args(typesString));
		}
		else
			return "Empty Array";

	case (ValueType::Map):
	{
		if (this->Array.size() > 0)
		{
			if (this->Array.size() % 2 != 0)
				return "Invalid Map (Odd number of Array elements)";

			return std::vformat(
				"Map<{}:{}>",
				std::make_format_args(
					Reflection::TypeAsString(this->Array[0].Type),
					Reflection::TypeAsString(this->Array[1].Type)
				));
		}
		else
			return "Empty Map";
	}
	}

	default:
	{
		std::string tName = TypeAsString(Type);
		return std::vformat(
			"GenericValue::ToString failed, Type was: {}",
			std::make_format_args(tName)
		);
	}
	}
}

std::string Reflection::GenericValue::AsString() const
{
	return Type == ValueType::String
		? String : throw("GenericValue was not a String, but was a " + Reflection::TypeAsString(Type));
}
bool Reflection::GenericValue::AsBool() const
{
	return Type == ValueType::Bool
		? (bool)this->Pointer
		: throw("GenericValue was not a Bool, but was a " + Reflection::TypeAsString(Type));
}
double Reflection::GenericValue::AsDouble() const
{
	return Type == ValueType::Double
		? *(double*)&this->Pointer
		: throw("GenericValue was not a Double, but was a " + Reflection::TypeAsString(Type));
}
int64_t Reflection::GenericValue::AsInteger() const
{
	// `|| ValueType::GameObject` because it's easier 14/09/2024
	return (Type == ValueType::Integer || Type == ValueType::GameObject)
		? (int64_t)this->Pointer
		: throw("GenericValue was not an Integer, but was a " + Reflection::TypeAsString(Type));
}
glm::mat4 Reflection::GenericValue::AsMatrix() const
{
	glm::mat4* mptr = (glm::mat4*)Pointer;

	return Type == ValueType::Matrix
		? *mptr
		: throw("GenericValue was not a Matrix, but was a " + Reflection::TypeAsString(Type));
}
std::vector<Reflection::GenericValue> Reflection::GenericValue::AsArray()
{
	if (Type == ValueType::Map)
	{
		if (Array.size() % 2 != 0)
			throw("Tried to convert a Map GenericValue to an Array, but it wasn't valid and had an odd number of Array elements");

		std::vector<GenericValue> array;

		for (size_t index = 1; index < array.size(); index++)
		{
			array.push_back(this->Array.at(index));
			index++;
		}

		return array;
	}

	return Type == ValueType::Array
		? Array
		: throw("GenericValue was not an Array, but was a " + Reflection::TypeAsString(Type));
}
std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> Reflection::GenericValue::AsMap() const
{
	if (Type != ValueType::Map)
		throw("GenericValue was not a Map, but was a " + Reflection::TypeAsString(Type));

	if (Array.size() % 2 != 0)
		throw("GenericValue was not a valid Map (odd number of Array elements)");

	/*
	std::unordered_map<GenericValue, GenericValue> map;

	for (size_t index = 0; index < Array.size(); index++)
	{
		map.insert(Array.at(index), Array.at(index + 1));
		index++;
	}

	return map;
	*/

	// 16/09/2024
	/*
	* 
	Severity	Code	Description	Project	File	Line	Suppression State	Details
	Error	C2280	'std::_Umap_traits<_Kty,_Ty,std::_Uhash_compare<_Kty,_Hasher,_Keyeq>,_Alloc,false>
	::_Umap_traits(const std::_Umap_traits<_Kty,_Ty,std::_Uhash_compare<_Kty,_Hasher,_Keyeq>,_Alloc,false> &)'
	: attempting to reference a deleted function
        with
        [
            _Kty=Reflection::GenericValue,
            _Ty=Reflection::GenericValue,
            _Hasher=std::hash<Reflection::GenericValue>,
            _Keyeq=std::equal_to<Reflection::GenericValue>,
            _Alloc=std::allocator<std::pair<const Reflection::GenericValue,Reflection::GenericValue>>
        ]	PhoenixEngine	C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120\include\xhash	396		


	What is blud yapping about?? :skull:
	*/
	throw("GenericValue::AsMap not implemented");
}

Reflection::PropertyMap& Reflection::Reflectable::GetProperties()
{
	return  s_Properties;
}

Reflection::FunctionMap& Reflection::Reflectable::GetFunctions()
{
	return  s_Functions;
}

bool Reflection::Reflectable::HasProperty(const std::string& name)
{
	if ( s_Properties.size() == 0)
		return false;
	return  s_Properties.find(name) !=  s_Properties.end();
}

bool Reflection::Reflectable::HasFunction(const std::string& name)
{
	return  s_Functions.find(name) !=  s_Functions.end();
}

Reflection::IProperty& Reflection::Reflectable::GetProperty(const std::string& name)
{
	if (!HasProperty(name))
		throw(std::vformat("(GetProperty) No Property named {}", std::make_format_args(name)));
	else
		return  s_Properties.find(name)->second;
}

Reflection::IFunction& Reflection::Reflectable::GetFunction(const std::string& name)
{
	if (!HasFunction(name))
		throw(std::vformat("(GetFunction) No Function named {}", std::make_format_args(name)));
	else
		return  s_Functions.find(name)->second;
}

Reflection::GenericValue Reflection::Reflectable::GetPropertyValue(const std::string& name)
{
	if (!HasProperty(name))
		throw(std::vformat("(GetPropertyValue) No Property named {}", std::make_format_args(name)));
	else
		return  s_Properties.find(name)->second.Get(dynamic_cast<Reflection::Reflectable*>(this));
}

void Reflection::Reflectable::SetPropertyValue(const std::string& name, GenericValue& value)
{
	if (!HasProperty(name))
		throw(std::vformat("(SetPropertyValue) No Property named {}", std::make_format_args(name)));
	else
		 s_Properties.find(name)->second.Set((Reflection::Reflectable*)this, value);
}

Reflection::GenericValue Reflection::Reflectable::CallFunction(const std::string& name, GenericValue input)
{
	if (!HasFunction(name))
		throw(std::vformat("(CallFunction) No Function named {}", std::make_format_args(name)));
	else
		return  s_Functions.find(name)->second.Func((Reflection::Reflectable*)this, input);
}

static bool s_DidInitReflection = false;

Reflection::Reflectable::Reflectable()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	//Reflectable::ApiReflection = new Reflection::Reflectable();
	
}

static std::string valueTypeNames[] =
{
		"Null",

		"Bool",
		"Integer",
		"Double",
		"String",

		"Color",
		"Vector3",
		"Matrix",

		"GameObject",

		"Array",
		"Map"
};

static_assert(
	std::size(valueTypeNames) == (static_cast<uint32_t>(Reflection::ValueType::_count)),
	"`ValueTypeNames` does not have the same number of elements as `ValueType`"
);

const std::string& Reflection::TypeAsString(ValueType t)
{
	return valueTypeNames[(int)t];
}
