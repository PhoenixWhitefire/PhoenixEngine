#include "Reflection.hpp"
#include "datatype/Vector3.hpp"
#include "datatype/Color.hpp"
#include "datatype/GameObject.hpp"

//Reflection::Reflectable* Reflection::Reflectable::ApiReflection = new Reflection::Reflectable();

Reflection::GenericValue::GenericValue()
	: Type(ValueType::Null)
{
}

Reflection::GenericValue::GenericValue(const std::string& str)
	: Type(ValueType::String)
{
	const char* cstr = str.c_str();
	size_t allocSize = strlen(cstr) + 1;

	this->Value = malloc(allocSize);

	if (!this->Value)
		throw(std::vformat(
			"Could not allocate {} bytes for ::GenericValue(string)", std::make_format_args(allocSize)
		));

	memcpy(this->Value, cstr, allocSize);
}

Reflection::GenericValue::GenericValue(const char* data)
	: Type(ValueType::String)
{
	size_t allocSize = strlen(data) + 1;

	this->Value = malloc(allocSize);

	if (!this->Value)
		throw(std::vformat(
			"Could not allocate {} bytes for ::GenericValue(const char*)", std::make_format_args(allocSize)
		));

	memcpy(this->Value, data, allocSize);
}

Reflection::GenericValue::GenericValue(bool b)
	: Type(ValueType::Bool), Value((void*)b)
{
}

Reflection::GenericValue::GenericValue(double d)
	: Type(ValueType::Double), Value(*(void**)&d)
{
}

Reflection::GenericValue::GenericValue(int64_t i)
	: Type(ValueType::Integer), Value((void*)i)
{
}

Reflection::GenericValue::GenericValue(uint32_t i)
	: Type(ValueType::Integer), Value((void*)static_cast<int64_t>(i))
{
}

Reflection::GenericValue::GenericValue(int i)
	: Type(ValueType::Integer), Value((void*)static_cast<int64_t>(i))
{
}

Reflection::GenericValue::GenericValue(const glm::mat4& m)
	: Type(ValueType::Matrix), Value(malloc(sizeof(m)))
{
	if (!this->Value)
		throw("Allocation error while constructing GenericValue from glm::mat4");

	memcpy(this->Value, &m, sizeof(m));
}

Reflection::GenericValue::GenericValue(const std::vector<GenericValue>& array)
	: Type(ValueType::Array)
{
	size_t allocSize = array.size() * sizeof(GenericValue);

	this->Value = malloc(allocSize);

	if (!this->Value)
		throw("Allocation error while constructing GenericValue from std::vector<GenericValue>");

	memcpy(this->Value, array.data(), allocSize);

	this->ArrayLength = array.size();
}

Reflection::GenericValue::GenericValue(const std::unordered_map<GenericValue, GenericValue>& map)
	: Type(ValueType::Map)
{
	std::vector<GenericValue> arr;
	arr.reserve(map.size() * 2);

	for (auto& it : map)
	{
		arr.push_back(it.first);
		arr.push_back(it.second);
	}

	size_t allocSize = arr.size() * sizeof(GenericValue);

	this->Value = (GenericValue*)malloc(allocSize);

	if (!this->Value)
		throw("Allocation error while constructing GenericValue from std::map<GenericValue, GenericValue>");

	memcpy(this->Value, arr.data(), allocSize);

	this->ArrayLength = arr.size();
}

std::string Reflection::GenericValue::ToString()
{
	switch (this->Type)
	{
	case (ValueType::Null):
		return "Null";

	case (ValueType::Bool):
		return (bool)this->Value ? "true" : "false";

	case (ValueType::Integer):
		return std::to_string((int64_t)this->Value);

	case (ValueType::Double):
		return std::to_string(*(double*)&this->Value);

	case (ValueType::String):
		return (const char*)this->Value;
	
	case (ValueType::Color):
		return Color(*this).ToString();

	case (ValueType::Vector3):
		return Vector3(*this).ToString();

	case (ValueType::GameObject):
	{
		GameObject* object = GameObject::GetObjectById(*(uint32_t*)&this->Value);
		return object ? object->GetFullName() : "NULL GameObject";
	}
	
	case (ValueType::Array):
	{
		std::vector<GenericValue> arr = this->AsArray();

		if (!arr.empty())
		{
			uint32_t numTypes = 0;
			std::string typesString = "";

			for (const Reflection::GenericValue& element : arr)
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
	}

	case (ValueType::Map):
	{
		std::vector<GenericValue> arr = this->AsArray();

		if (!arr.empty())
		{
			if (arr.size() % 2 != 0)
				return "Invalid Map (Odd number of Array elements)";
			
			return std::vformat(
				"Map<{}:{}>",
				std::make_format_args(
					Reflection::TypeAsString(arr[0].Type),
					Reflection::TypeAsString(arr[1].Type)
				));
		}
		else
			return "Empty Map";
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
		? (const char*)this->Value : throw("GenericValue was not a String, but was a " + Reflection::TypeAsString(Type));
}
bool Reflection::GenericValue::AsBool() const
{
	return Type == ValueType::Bool
		? (bool)this->Value
		: throw("GenericValue was not a Bool, but was a " + Reflection::TypeAsString(Type));
}
double Reflection::GenericValue::AsDouble() const
{
	return Type == ValueType::Double
		? *(double*)&this->Value
		: throw("GenericValue was not a Double, but was a " + Reflection::TypeAsString(Type));
}
int64_t Reflection::GenericValue::AsInteger() const
{
	// `|| ValueType::GameObject` because it's easier 14/09/2024
	return (Type == ValueType::Integer || Type == ValueType::GameObject)
		? (int64_t)this->Value
		: throw("GenericValue was not an Integer, but was a " + Reflection::TypeAsString(Type));
}
glm::mat4 Reflection::GenericValue::AsMatrix() const
{
	glm::mat4* mptr = (glm::mat4*)this->Value;

	return Type == ValueType::Matrix
		? *mptr
		: throw("GenericValue was not a Matrix, but was a " + Reflection::TypeAsString(Type));
}
std::vector<Reflection::GenericValue> Reflection::GenericValue::AsArray() const
{
	std::vector<GenericValue> array;
	array.reserve(this->ArrayLength);

	Reflection::GenericValue* first = (Reflection::GenericValue*)this->Value;

	if (Type == ValueType::Map)
	{
		if (this->ArrayLength % 2 != 0)
			throw("Tried to convert a Map GenericValue to an Array, but it wasn't valid and had an odd number of Array elements");

		for (size_t index = 1; index < this->ArrayLength; index++)
		{
			array.push_back(first[index]);
			index++;
		}

		return array;
	}
	else
		for (uint32_t i = 0; i < this->ArrayLength; i++)
			array.push_back(first[i]);

	return this->Type == ValueType::Array
		? array
		: throw("GenericValue was not an Array, but was a " + Reflection::TypeAsString(Type));
}
std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> Reflection::GenericValue::AsMap() const
{
	throw("GenericValue::AsMap not implemented");

	/*
	if (Type != ValueType::Map)
		throw("GenericValue was not a Map, but was a " + Reflection::TypeAsString(Type));

	//if (Array.size() % 2 != 0)
	//	throw("GenericValue was not a valid Map (odd number of Array elements)");

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
	
	throw("GenericValue::AsMap not implemented");
	*/
}

Reflection::GenericValue::~GenericValue()
{
	// "A breakpoint instruction (__debugbreak() or similar) was hit"
	//if (this->Type == Reflection::ValueType::String)
		//free(this->Value);
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
	if (s_Properties.empty())
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
