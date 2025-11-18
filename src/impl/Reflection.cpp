// C++ interop transmission channel

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/euler_angles.hpp>

#include "Reflection.hpp"
#include "datatype/GameObject.hpp"
#include "datatype/Color.hpp"
#include "Memory.hpp"

#define GV_SSO sizeof(Reflection::GenericValue::Val.StrSso)
static_assert(GV_SSO == 12);

Reflection::GenericValue::GenericValue()
	: Type(ValueType::Null)
{
}

static void fromString(Reflection::GenericValue& G, const char* Data)
{
	G.Type = Reflection::ValueType::String;

	if (G.Size > GV_SSO)
	{
		G.Val.Str = (char*)Memory::Alloc(G.Size, Memory::Category::Reflection);

		if (!G.Val.Str)
			RAISE_RTF("Failed to allocate {} bytes for string in fromString", G.Size);

		memcpy(G.Val.Str, Data, G.Size);
	}
	else
		// store it directly
		memcpy(G.Val.StrSso, Data, G.Size);
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
	this->Size = strlen(data);
	fromString(*this, data);
}

Reflection::GenericValue::GenericValue(bool b)
	: Type(ValueType::Boolean)
{
	Val.Bool = b;
}

Reflection::GenericValue::GenericValue(double d)
	: Type(ValueType::Double)
{
	Val.Double = d;
}

Reflection::GenericValue::GenericValue(int64_t i)
	: Type(ValueType::Integer)
{
	Val.Int = i;
}

Reflection::GenericValue::GenericValue(uint32_t i)
	: Type(ValueType::Integer)
{
	Val.Int = i;
}

Reflection::GenericValue::GenericValue(int i)
	: Type(ValueType::Integer)
{
	Val.Int = i;
}

static void fromMatrix(Reflection::GenericValue& G, const glm::mat4& M)
{
	G.Type = Reflection::ValueType::Matrix;
	G.Val.Ptr = Memory::Alloc(sizeof(M), Memory::Category::Reflection);
	G.Size = sizeof(M);

	if (!G.Val.Ptr)
		RAISE_RTF("Failed to allocate {} bytes in fromMatrix", sizeof(M));

	memcpy(G.Val.Ptr, &M, sizeof(M));
}

Reflection::GenericValue::GenericValue(glm::vec2 v)
	: Type(Reflection::ValueType::Vector2)
{
	Val.Vec2 = v;
}

Reflection::GenericValue::GenericValue(const glm::vec3& v)
	: Type(Reflection::ValueType::Vector3)
{
	Val.Vec3 = v;
}

Reflection::GenericValue::GenericValue(const glm::mat4& m)
	: Type(ValueType::Matrix)
{
	fromMatrix(*this, m);
}

Reflection::GenericValue::GenericValue(const Reflection::GenericFunction& gf) // damn
	: Type(ValueType::Function)
{
	Val.Func = new GenericFunction(gf); // damm
}

static void fromArray(Reflection::GenericValue& G, std::span<const Reflection::GenericValue> Array)
{
	G.Type = Reflection::ValueType::Array;

	size_t allocSize = Array.size() * sizeof(G);

	G.Val.Ptr = Memory::Alloc(allocSize, Memory::Category::Reflection);

	if (!G.Val.Ptr)
		RAISE_RTF("Failed to allocate {} bytes in fromArray (length {}, GV Size {})", allocSize, Array.size(), sizeof(G));

	for (uint32_t i = 0; i < Array.size(); i++)
		// placement-new to avoid 1 excess layer of indirection
		new (&((Reflection::GenericValue*)G.Val.Ptr)[i]) Reflection::GenericValue(Array[i]);

	G.Size = Array.size();
}

Reflection::GenericValue::GenericValue(const std::span<GenericValue>& array)
	: Type(ValueType::Array)
{
	fromArray(*this, array);
}

Reflection::GenericValue::GenericValue(const std::vector<GenericValue>& array)
	: Type(ValueType::Array)
{
	fromArray(*this, std::span(array));
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

	this->Val.Ptr = (GenericValue*)Memory::Alloc(allocSize, Memory::Category::Reflection);

	if (!this->Val.Ptr)
		RAISE_RTF("Failed to allocate {} bytes in construction from std::unordered_map (length {}, GV Size {})", allocSize, arr.size(), sizeof(GenericValue));

	memcpy(this->Val.Ptr, arr.data(), allocSize);

	this->Size = static_cast<uint32_t>(arr.size());
}

void Reflection::GenericValue::CopyInto(GenericValue& Target, const GenericValue& Source)
{
	Target.Type = Source.Type;
	Target.Size = Source.Size;

	switch (Target.Type)
	{
	case ValueType::String:
	{
		std::string_view str = Source.AsStringView();
		fromString(Target, str.data());
		break;
	}
	case ValueType::Color:
	{
		Target.Val.Vec3 = Source.Val.Vec3;
		break;
	}
	case ValueType::Vector3:
	{
		Target.Val.Vec3 = Source.Val.Vec3;
		break;
	}
	case ValueType::Matrix:
	{
		fromMatrix(Target, Source.AsMatrix());
		break;
	}
	case ValueType::Array:
	{
		fromArray(Target, Source.AsArray());
		break;
	}
	case ValueType::Map:
	{
		fromArray(Target, std::span((Reflection::GenericValue*)Source.Val.Ptr, Source.Size));
		Target.Type = ValueType::Map;
		break;
	}

	case ValueType::Boolean: case ValueType::Double: case ValueType::Integer:

	default:
		Target.Val.Ptr = Source.Val.Ptr;
	}
}

Reflection::GenericValue::GenericValue(GenericValue&& Other)
{
	if (this == &Other)
		return;

	Reset();
	this->Type = Other.Type;
	this->Val = Other.Val;
	this->Size = Other.Size;

	Other.Type = ValueType::Null;
	Other.Val = {};
	Other.Size = 0;
}

Reflection::GenericValue::GenericValue(const GenericValue& Other)
{
	CopyInto(*this, Other);
}

std::string Reflection::GenericValue::ToString() const
{
	switch (this->Type)
	{
	case ValueType::Null:
		return "Null";

	case ValueType::Boolean:
		return Val.Bool ? "true" : "false";

	case ValueType::Integer:
		return std::to_string(AsInteger());

	case ValueType::Double:
		return std::to_string(AsDouble());

	case ValueType::String:
	{
		if (this->Size > GV_SSO)
			return std::string(Val.Str, this->Size);
		else
			return Val.StrSso;
	}
	
	case ValueType::Color:
		return Color(*this).ToString();

	case ValueType::Vector2:
	{
		const glm::vec2& v = AsVector2();
		return std::format("{}, {}", v.x, v.y);
	}

	case ValueType::Vector3:
	{
		const glm::vec3& v = AsVector3();
		return std::format("{}, {}, {}", v.x, v.y, v.z);
	}

	case ValueType::GameObject:
	{
		GameObject* object = GameObject::FromGenericValue(*this);
		return object ? object->GetFullName() : "NULL GameObject";
	}
	
	case ValueType::Array:
	{
		std::span<GenericValue> arr = this->AsArray();

		if (!arr.empty())
		{
			uint32_t numTypes = 0;
			std::string typesString = "";

			for (const GenericValue& element : arr)
			{
				numTypes++;
				if (numTypes > 4)
				{
					typesString += "...|";
					break;
				}
				else
					typesString += TypeAsString(element.Type) + "|";
			}

			typesString = typesString.substr(0, typesString.size() - 1);

			return std::format("Array<{}>", typesString);
		}
		else
			return "Empty Array";
	}

	case ValueType::Map:
	{
		std::span<GenericValue> arr = this->AsArray();

		if (!arr.empty())
		{
			if (arr.size() % 2 != 0)
				return "Invalid Map (Odd number of Array elements)";
			
			return std::format(
				"Map<{}:{}>",
				TypeAsString(arr[0].Type),
				TypeAsString(arr[1].Type)
			);
		}
		else
			return "Empty Map";
	}

	case ValueType::Matrix:
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

		return std::format(
			"Pos: ( {}, {}, {} ), Ang: ( {}, {}, {} )",
			pos[0], pos[1], pos[2], rotdegs[0], rotdegs[1], rotdegs[2]
		);
	}

	default:
		return std::format(
			"ToString case not implemented for Type '{}'",
			TypeAsString(Type)
		);
	}
}

// `::AsInteger` and `::AsDouble` have special errors
#define WRONG_TYPE() RAISE_RTF("Called {} but Generic Value was a {}", __FUNCTION__, TypeAsString(Type))

std::string_view Reflection::GenericValue::AsStringView() const
{
	if (Type != ValueType::String)
		WRONG_TYPE();
	else
		if (Size > GV_SSO)
			return std::string_view(Val.Str, Size);
		else
			return std::string_view(Val.StrSso, Size);
}
bool Reflection::GenericValue::AsBoolean() const
{
	return Type == ValueType::Boolean
		? Val.Bool
		: WRONG_TYPE();
}
double Reflection::GenericValue::AsDouble() const
{
	if (Type == ValueType::Double)
		return Val.Double;

	else if (Type == ValueType::Integer)
		return (double)Val.Int;

	// special error message
	RAISE_RTF("GenericValue was not a number (Integer/ Double ), but instead a {}", TypeAsString(Type));
}
int64_t Reflection::GenericValue::AsInteger() const
{
	if (Type == ValueType::Integer)
		return Val.Int;

	else if (Type == ValueType::Double)
		return (int64_t)Val.Double;

	RAISE_RTF("GenericValue was not a number ( Integer /Double), but instead a {}", TypeAsString(Type));
}

const glm::vec2 Reflection::GenericValue::AsVector2() const
{
	return Type == ValueType::Vector2
		? Val.Vec2
		: WRONG_TYPE();
}
glm::vec3& Reflection::GenericValue::AsVector3() const
{
	return Type == ValueType::Vector3
		? const_cast<glm::vec3&>(Val.Vec3)
		: WRONG_TYPE();
}
glm::mat4& Reflection::GenericValue::AsMatrix() const
{
	glm::mat4* mptr = (glm::mat4*)this->Val.Ptr;

	return Type == ValueType::Matrix
		? *mptr
		: WRONG_TYPE();
}
Reflection::GenericFunction& Reflection::GenericValue::AsFunction() const
{
	return Type == ValueType::Function
		? *Val.Func
		: WRONG_TYPE();
}
std::span<Reflection::GenericValue> Reflection::GenericValue::AsArray() const
{
	if (this->Type != Reflection::ValueType::Array)
		RAISE_RTF("GenericValue was not an Array, but instead a {}", TypeAsString(Type));
	
	Reflection::GenericValue* first = (Reflection::GenericValue*)this->Val.Ptr;
	return std::span(first, Size);
}
std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> Reflection::GenericValue::AsMap() const
{
	RAISE_RT("GenericValue::AsMap not implemented");
}

#undef WRONG_TYPE

static void deallocGv(Reflection::GenericValue* gv)
{
	using Reflection::ValueType;

	// values actually being transmitted shouldnt ever have the optional flag,
	// that's only for type definitions
	assert((uint8_t)gv->Type <= (uint8_t)ValueType::Null);

	switch (gv->Type)
	{
	case ValueType::Matrix:
	{
		Memory::Free(gv->Val.Ptr);
		break;
	}
	case ValueType::Array: case ValueType::Map:
	{
		for (uint32_t i = 0; i < gv->Size; i++)
			// de-alloc potential heap buffers of elements first
			(((Reflection::GenericValue*)gv->Val.Ptr)[i]).~GenericValue();
		
		Memory::Free(gv->Val.Ptr);

		break;
	}
	case ValueType::String:
	{
		if (gv->Size > GV_SSO)
			Memory::Free(gv->Val.Str);
		break;
	}
	case ValueType::Function:
	{
		delete gv->Val.Func;
		break;
	}

	default: {}
	}
}

void Reflection::GenericValue::Reset()
{
	deallocGv(this);

	Type = Reflection::ValueType::Null;
	Val = {};
	Size = 0;
}

Reflection::GenericValue::~GenericValue()
{
	deallocGv(this);
}

bool Reflection::GenericValue::operator==(const Reflection::GenericValue& Other) const
{
	if (this->Type != Other.Type)
		return false;
	if (this->Size != Other.Size)
		return false;

	switch (Type)
	{
	case ValueType::Null:
		return true;

	case ValueType::String:
	{
		if (this->Size > GV_SSO)
		{
			for (size_t i = 0; i < this->Size; i++)
				if (this->Val.Str[i] != Other.Val.Str[i])
					return false;
			
			return true;
		}
		else
			return memcmp(this->Val.StrSso, Other.Val.StrSso, GV_SSO) == 0;
	}

	case ValueType::Matrix:
	{
		return memcmp(this->Val.Ptr, Other.Val.Ptr, sizeof(glm::mat4));
	}

	case ValueType::Array:
	{
		std::span<GenericValue> myArr = this->AsArray();
		std::span<GenericValue> themArr = Other.AsArray();

		for (uint32_t i = 0; i < myArr.size(); i++)
			if (myArr[i] != themArr[i])
				return false;

		return true;
	}

	case ValueType::Boolean:
	{
		return Val.Bool == Other.Val.Bool;
	}
	case ValueType::Integer:
	{
		return Val.Int == Other.Val.Int;
	}
	case ValueType::Double:
	{
		return Val.Double == Other.Val.Double;
	}
	case ValueType::Vector2:
	{
		return Val.Vec2 == Other.Val.Vec2;
	}
	case ValueType::Color: case ValueType::Vector3:
	{
		return Val.Vec3 == Other.Val.Vec3;
	}

	default:
		return memcmp(&Val, &Other.Val, sizeof(ValueData));
	}
}

Reflection::GenericValue& Reflection::GenericValue::operator=(Reflection::GenericValue&& Other)
{
	if (this == &Other)
		return *this;

	Reset();
	this->Type = Other.Type;
	this->Val = Other.Val;
	this->Size = Other.Size;

	Other.Type = ValueType::Null;
	Other.Val = {};
	Other.Size = 0;

	return *this;
}

Reflection::GenericValue& Reflection::GenericValue::operator=(const Reflection::GenericValue& Other)
{
	CopyInto(*this, Other);

	return *this;
}

static std::string_view BaseNames[] =
{
	"<ERROR_UNINITIALIZED>",

	"Boolean",
	"Integer",
	"Double",
	"String",

	"Color",
	"Vector2",
	"Vector3",
	"Matrix",

	"GameObject",
	"Function",

	"Array",
	"Map"
};

static_assert(
	std::size(BaseNames) == (size_t)Reflection::ValueType::__lastBase,
	"'ValueTypeNames' does not have the same number of elements as 'ValueType'"
);

std::string Reflection::TypeAsString(ValueType vt)
{
	if (vt == ValueType::Null)
		return "Null";

	if (vt < ValueType::Null)
		return std::string(BaseNames[vt]);

	// everything greater than Null means optional
	// Null + Boolean = Boolean? (optional boolean)
	return std::string(BaseNames[vt - ValueType::Null]) + "?";
}

bool Reflection::TypeFits(ValueType Target, ValueType Value)
{
	assert(Target != ValueType::Null);
	uint8_t base = Target & ~ValueType::Null;

	return (Value == base) || ((Target & ValueType::Null) && Value == ValueType::Null);
}
