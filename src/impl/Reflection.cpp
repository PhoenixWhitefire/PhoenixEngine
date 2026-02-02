// C++ interop transmission channel

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/euler_angles.hpp>

#include "Reflection.hpp"
#include "datatype/GameObject.hpp"
#include "datatype/Color.hpp"
#include "Memory.hpp"

#define GV_SSO sizeof(Reflection::GenericValue::Val.StrSso)
static_assert(GV_SSO == 12);

static void fromString(Reflection::GenericValue& G, const char* Data)
{
	G.Type = Reflection::ValueType::String;

	if (G.Size + 1 > GV_SSO)
	{
		G.Val.Str = (char*)Memory::Alloc(G.Size + 1, Memory::Category::Reflection);

		if (!G.Val.Str)
			RAISE_RTF("Failed to allocate {} bytes for string in fromString", G.Size);

		memcpy(G.Val.Str, Data, G.Size);
		G.Val.Str[G.Size] = 0;
	}
	else
		// store it directly
		memcpy(G.Val.StrSso, Data, G.Size);
}

Reflection::GenericValue::GenericValue(const std::string_view& str)
	: Type(ValueType::String)
{
	assert(str.size() <= UINT32_MAX);

	this->Size = (uint32_t)str.size();
	fromString(*this, str.data());
}

Reflection::GenericValue::GenericValue(const std::string& str)
	: Type(ValueType::String)
{
	assert(str.size() <= UINT32_MAX);

	this->Size = (uint32_t)str.size();
	fromString(*this, str.data());
}

Reflection::GenericValue::GenericValue(const char* data)
	: Type(ValueType::String)
{
	assert(strlen(data) <= UINT32_MAX);

	this->Size = (uint32_t)strlen(data);
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
	G.Val.Mat = (glm::mat4*)Memory::Alloc(sizeof(M), Memory::Category::Reflection);
	G.Size = sizeof(M);

	if (!G.Val.Mat)
		RAISE_RTF("Failed to allocate {} bytes in fromMatrix", sizeof(M));

	memcpy(G.Val.Mat, &M, sizeof(M));
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

static void fromArray(Reflection::GenericValue& G, const std::span<const Reflection::GenericValue>& Array)
{
	G.Type = Reflection::ValueType::Array;

	size_t allocSize = Array.size() * sizeof(G);
	if (allocSize == 0)
	{
		G.Val.Array = nullptr;
		return;
	}

	assert(allocSize <= UINT32_MAX);
	G.Val.Array = (Reflection::GenericValue*)Memory::Alloc((uint32_t)allocSize, Memory::Category::Reflection);

	if (!G.Val.Array)
		RAISE_RTF("Failed to allocate {} bytes in fromArray (length {}, GV Size {})", allocSize, Array.size(), sizeof(G));

	for (size_t i = 0; i < Array.size(); i++)
		// placement-new to avoid 1 excess layer of indirection
		new (&G.Val.Array[i]) Reflection::GenericValue(Array[i]);

	assert(Array.size() <= UINT32_MAX);
	G.Size = (uint32_t)Array.size();
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
{
	std::vector<GenericValue> arr;
	arr.reserve(map.size() * 2);

	for (auto& it : map)
	{
		arr.push_back(it.first);
		arr.push_back(it.second);
	}

	fromArray(*this, arr);
	this->Type = ValueType::Map;
}

Reflection::GenericValue Reflection::GenericValue::Null()
{
	return {};
}

void Reflection::GenericValue::CopyInto(GenericValue& Target, const GenericValue& Source)
{
	Target.Type = Source.Type;
	Target.Size = Source.Size;

	switch (Target.Type)
	{
	case ValueType::Null:
		return;
	case ValueType::String:
	{
		std::string_view str = Source.AsStringView();
		fromString(Target, str.data());
		break;
	}
	case ValueType::Color: case ValueType::Vector3:
	{
		Target.Val.Vec3 = Source.Val.Vec3;
		break;
	}
	case ValueType::Vector2:
	{
		Target.Val.Vec2 = Source.Val.Vec2;
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
		fromArray(Target, std::span(Source.Val.Array, Source.Size));
		Target.Type = ValueType::Map;
		break;
	}

	case ValueType::Boolean:
	{
		Target.Val.Bool = Source.Val.Bool;
		break;
	}
	case ValueType::Double:
	{
		Target.Val.Double = Source.Val.Double;
		break;
	}
	case ValueType::Integer: case ValueType::GameObject:
	{
		Target.Val.Int = Source.Val.Int;
		break;
	}
	case ValueType::EventSignal:
	{
		Target.Val.Event = Source.Val.Event;
		break;
	}
	case ValueType::InputEvent:
	{
		Target.Val.Input = Source.Val.Input;
		break;
	}
	default:
	{
		assert(false);
		Target.Val.Ptr = Source.Val.Ptr;
	}
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
		if (this->Size + 1 > GV_SSO)
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
	{
		assert(false);
		return TypeAsString(Type);
	}
	}
}

// `::AsInteger` and `::AsDouble` have special errors
#define WRONG_TYPE() RAISE_RTF("Called {} but Generic Value was a {}", __FUNCTION__, TypeAsString(Type))

std::string Reflection::GenericValue::AsString() const
{
	if (Type != ValueType::String)
		WRONG_TYPE();
	else
		if (Size + 1 > GV_SSO)
			return std::string(Val.Str, Size);
		else
			return std::string(Val.StrSso, Size);
}
std::string_view Reflection::GenericValue::AsStringView() const
{
	if (Type != ValueType::String)
		WRONG_TYPE();
	else
		if (Size + 1 > GV_SSO)
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
	return Type == ValueType::Matrix
		? *Val.Mat
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

	return std::span(Val.Array, Size);
}
std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> Reflection::GenericValue::AsMap() const
{
	if (Size % 2 != 0)
		RAISE_RTF("Map had an uneven number of array elements ({})", Size);

	std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> map;
	for (uint32_t i = 0; i < Size; i += 2)
		map[Val.Array[i]] = Val.Array[i + 1];

	return map;
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
		Memory::Free(gv->Val.Mat);
		break;
	}
	case ValueType::Array: case ValueType::Map:
	{
		for (uint32_t i = 0; i < gv->Size; i++)
			// de-alloc potential heap buffers of elements first
			gv->Val.Array[i].~GenericValue();
		
		Memory::Free(gv->Val.Array);
		break;
	}
	case ValueType::String:
	{
		if (gv->Size + 1 > GV_SSO)
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
		if (this->Size + 1 > GV_SSO)
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
		return this->AsMatrix() == Other.AsMatrix();
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
	case ValueType::Integer: case ValueType::GameObject:
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
		return memcmp(&Val, &Other.Val, sizeof(ValueData)) == 0;
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
	"<Erroneous>",

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
	"Map",

	"EventSignal"
};

static_assert(
	std::size(BaseNames) == (size_t)Reflection::ValueType::__lastBase,
	"'ValueTypeNames' does not have the same number of elements as 'ValueType'"
);

std::string Reflection::TypeAsString(ValueType vt)
{
	if (vt == ValueType::Null)
		return "Null";

	if (vt == ValueType::Any)
		return "Any";

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

	return (Value == base) || ((Target & ValueType::Null) && Value == ValueType::Null) || Target == ValueType::Any;
}
