#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/euler_angles.hpp>

#include "Reflection.hpp"
#include "datatype/GameObject.hpp"
#include "datatype/Vector3.hpp"
#include "datatype/Color.hpp"
#include "Memory.hpp"

Reflection::GenericValue::GenericValue()
	: Type(ValueType::Null)
{
}

static void fromString(Reflection::GenericValue& G, const char* CStr)
{
	G.Value = Memory::Alloc(G.Size + 1, Memory::Category::Reflection);

	if (!G.Value)
		throw("Failed to allocate enough space for string in fromString");

	memcpy(G.Value, CStr, G.Size);
	((char*)G.Value)[G.Size] = 0;
}

Reflection::GenericValue::GenericValue(const std::string_view& str)
	: Type(ValueType::String)
{
	this->Size = str.size();
	fromString(*this, str.data());
}

Reflection::GenericValue::GenericValue(const std::string& str)
	: Type(ValueType::String)
{
	this->Size = str.size();
	fromString(*this, str.data());
}

Reflection::GenericValue::GenericValue(const char* data)
	: Type(ValueType::String)
{
	this->Size = strlen(data) + 1;
	fromString(*this, data);
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

static void fromMatrix(Reflection::GenericValue& G, const glm::mat4& M)
{
	G.Value = Memory::Alloc(sizeof(M), Memory::Category::Reflection);
	G.Size = sizeof(M);

	if (!G.Value)
		throw("Allocation error while constructing GenericValue from glm::mat4");

	memcpy(G.Value, &M, sizeof(M));
}

Reflection::GenericValue::GenericValue(const glm::mat4& m)
	: Type(ValueType::Matrix)
{
	fromMatrix(*this, m);
}

static void fromArray(Reflection::GenericValue& G, const std::vector<Reflection::GenericValue>& Array)
{
	size_t allocSize = Array.size() * sizeof(G);

	G.Value = Memory::Alloc(allocSize, Memory::Category::Reflection);

	if (!G.Value)
		throw("Allocation error while constructing GenericValue from std::vector<GenericValue>");

	memcpy(G.Value, Array.data(), allocSize);

	G.Size = Array.size();
}

Reflection::GenericValue::GenericValue(const std::vector<GenericValue>& array)
	: Type(ValueType::Array)
{
	fromArray(*this, array);
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

	this->Value = (GenericValue*)Memory::Alloc(allocSize, Memory::Category::Reflection);

	if (!this->Value)
		throw("Allocation error while constructing GenericValue from std::map<GenericValue, GenericValue>");

	memcpy(this->Value, arr.data(), allocSize);

	this->Size = static_cast<uint32_t>(arr.size());
}

void Reflection::GenericValue::CopyInto(GenericValue& Target, const GenericValue& Source)
{
	Target.Type = Source.Type;

	switch (Target.Type)
	{
	case (ValueType::String):
	{
		std::string str = Source.AsString();
		Target.Size = str.size();
		fromString(Target, str.data());
		break;
	}
	case (ValueType::Color):
	{
		Target.Value = new Color(Source);
		break;
	}
	case (ValueType::Vector3):
	{
		Target.Value = new Vector3(Source);
		break;
	}
	case (ValueType::Matrix):
	{
		fromMatrix(Target, Source.AsMatrix());
		break;
	}
	case (ValueType::Array):
	{
		fromArray(Target, Source.AsArray());
		break;
	}

	default:
		Target.Value = Source.Value;
	}
}

Reflection::GenericValue::GenericValue(GenericValue&& Other)
{
	CopyInto(*this, Other);
}

Reflection::GenericValue::GenericValue(const GenericValue& Other)
{
	CopyInto(*this, Other);
}

/*
Reflection::GenericValue::GenericValue(GenericValue&& Other)
{
	CopyInto(*this, Other);
}
*/

std::string Reflection::GenericValue::ToString() const
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
					typesString += std::string(Reflection::TypeAsString(element.Type)) + "|";
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

	case (ValueType::Matrix):
	{
		glm::mat4 mat = this->AsMatrix();

		float pos[3] =
		{
			mat[3][0],
			mat[3][1],
			mat[3][2]
		};

		glm::vec3 rotrads{};

		glm::extractEulerAngleXYZ(mat, rotrads.x, rotrads.y, rotrads.z);

		float rotdegs[3] =
		{
			glm::degrees(rotrads.x),
			glm::degrees(rotrads.y),
			glm::degrees(rotrads.z)
		};

		return std::vformat(
			"Pos: ( {}, {}, {} ), Ang: ( {}, {}, {} )",
			std::make_format_args(pos[0], pos[1], pos[2], rotdegs[0], rotdegs[1], rotdegs[2])
		);
	}

	default:
	{
		const std::string_view& tName = TypeAsString(Type);
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
		? (const char*)this->Value : throw("GenericValue was not a String, but was a " + std::string(Reflection::TypeAsString(Type)));
}
bool Reflection::GenericValue::AsBool() const
{
	return Type == ValueType::Bool
		? (bool)this->Value
		: throw("GenericValue was not a Bool, but was a " + std::string(Reflection::TypeAsString(Type)));
}
double Reflection::GenericValue::AsDouble() const
{
	return Type == ValueType::Double
		? *(double*)&this->Value
		: throw("GenericValue was not a Double, but was a " + std::string(Reflection::TypeAsString(Type)));
}
int64_t Reflection::GenericValue::AsInteger() const
{
	// `|| ValueType::GameObject` because it's easier 14/09/2024
	return (Type == ValueType::Integer || Type == ValueType::GameObject)
		? (int64_t)this->Value
		: throw("GenericValue was not an Integer, but was a " + std::string(Reflection::TypeAsString(Type)));
}
glm::mat4& Reflection::GenericValue::AsMatrix() const
{
	glm::mat4* mptr = (glm::mat4*)this->Value;

	return Type == ValueType::Matrix
		? *mptr
		: throw("GenericValue was not a Matrix, but was a " + std::string(Reflection::TypeAsString(Type)));
}
std::vector<Reflection::GenericValue> Reflection::GenericValue::AsArray() const
{
	std::vector<GenericValue> array;
	array.reserve(this->Size);

	Reflection::GenericValue* first = (Reflection::GenericValue*)this->Value;

	if (Type == ValueType::Map)
	{
		if (this->Size % 2 != 0)
			throw("Tried to convert a Map GenericValue to an Array, but it wasn't valid and had an odd number of Array elements");

		for (size_t index = 1; index < this->Size; index++)
		{
			array.push_back(first[index]);
			index++;
		}

		return array;
	}
	else
		for (uint32_t i = 0; i < this->Size; i++)
			array.push_back(first[i]);

	return this->Type == ValueType::Array
		? array
		: throw("GenericValue was not an Array, but was a " + std::string(Reflection::TypeAsString(Type)));
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
	switch (this->Type)
	{
	case (Reflection::ValueType::String):
	{
		Memory::Free(this->Value);
		break;
	}
	case (Reflection::ValueType::Matrix):
	{
		Memory::Free(this->Value);
		break;
	}
	case (Reflection::ValueType::Color):
	{
		delete (Color*)this->Value;
		break;
	}
	case (Reflection::ValueType::Vector3):
	{
		delete (Vector3*)this->Value;
		break;
	}
	case (Reflection::ValueType::Array):
	{
		Memory::Free(this->Value);
		break;
	}
	case (Reflection::ValueType::Map):
	{
		Memory::Free(this->Value);
		break;
	}
	}
}

bool Reflection::GenericValue::operator==(const Reflection::GenericValue& Other) const
{
	if (this->Type != Other.Type)
		return false;

	switch (Type)
	{
	case ValueType::Null:
		return true;

	case ValueType::Bool:
	case ValueType::Integer:
	case ValueType::Double:
	case ValueType::GameObject:
		return (size_t)this->Value == (size_t)Other.Value;

	case ValueType::String:
	{
		std::string myStr = this->AsString();
		std::string themStr = Other.AsString();

		return myStr == themStr;
	}

	case ValueType::Color:
	{
		Color myCol{ *this };
		Color themCol{ Other };

		return myCol == themCol;
	}

	case ValueType::Vector3:
	{
		Vector3 myCol{ *this };
		Vector3 themCol{ Other };

		return myCol == themCol;
	}

	case ValueType::Matrix:
	{
		glm::mat4 myMtx = this->AsMatrix();
		glm::mat4 themMtx = Other.AsMatrix();

		return myMtx == themMtx;
	}

	case ValueType::Array:
	{
		if (this->Size != Other.Size)
			return false;

		std::vector<GenericValue> myArr = this->AsArray();
		std::vector<GenericValue> themArr = Other.AsArray();

		for (uint32_t i = 0; i < myArr.size(); i++)
			if (!(myArr[i] == themArr[i]))
				return false;

		return true;
	}

	default:
		throw("No defined equality for GenericValues of type " + std::string(TypeAsString(Type)));
	}
}

static std::string_view ValueTypeNames[] =
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
	std::size(ValueTypeNames) == (size_t)Reflection::ValueType::__count,
	"'ValueTypeNames' does not have the same number of elements as 'ValueType'"
);

const std::string_view& Reflection::TypeAsString(ValueType t)
{
	return ValueTypeNames[(int)t];
}
