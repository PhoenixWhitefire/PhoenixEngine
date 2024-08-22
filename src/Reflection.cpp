#include"Reflection.hpp"
#include"datatype/Vector3.hpp"
#include"datatype/Color.hpp"
#include"datatype/GameObject.hpp"

Reflection::ReflectionInfo* Reflection::Reflectable::ApiReflection = new Reflection::ReflectionInfo();

Reflection::GenericValue::GenericValue()
	: Type(ValueType::None), Pointer(nullptr)
{
}

Reflection::GenericValue::GenericValue(std::string const& str)
	: Type(ValueType::String), String(str), Pointer(nullptr)
{
}

Reflection::GenericValue::GenericValue(bool b)
	: Type(ValueType::Bool), Bool(b), Pointer(nullptr)
{
}

Reflection::GenericValue::GenericValue(double d)
	: Type(ValueType::Double), Double(d), Pointer(nullptr)
{
}

Reflection::GenericValue::GenericValue(int i)
	: Type(ValueType::Integer), Integer(i), Pointer(nullptr)
{
}

Reflection::GenericValue::GenericValue(uint32_t i)
	: Type(ValueType::Integer), Integer(i), Pointer(nullptr)
{
}

std::string Reflection::GenericValue::ToString() const
{
	switch (this->Type)
	{
	case (ValueType::String):
		return this->String;

	case (ValueType::Bool):
		return this->Bool ? "true" : "false";

	case (ValueType::Double):
		return std::to_string(this->Double);

	case (ValueType::Integer):
		return std::to_string(this->Integer);

	case (ValueType::Color):
		return Color(*this).ToString();

	case (ValueType::Vector3):
		return Vector3().ToString();

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
	return this->Type == ValueType::String ? this->String : throw("GenericValue was not a String");
}
bool Reflection::GenericValue::AsBool() const
{
	return this->Type == ValueType::Bool ? this->Bool : throw("GenericValue was not a Bool");
}
double Reflection::GenericValue::AsDouble() const
{
	return this->Type == ValueType::Double ? this->Double : throw("GenericValue was not a Double");
}
int Reflection::GenericValue::AsInt() const
{
	return this->Type == ValueType::Integer ? this->Integer : throw("GenericValue was not an Integer");
}

void Reflection::BaseReflectable::pointlessPolymorhpismRequirementMeeter()
{
	throw("I think the set of events leading up to the moment of me writing this have irreversibly removed several years off my lifespan.");
}

Reflection::IProperty::IProperty
(
	Reflection::ValueType type,
	std::function<Reflection::GenericValue(Reflection::BaseReflectable*)> getter,
	std::function<void(Reflection::BaseReflectable*, Reflection::GenericValue)> setter
) : Type(type), Get(getter), Set(setter), Settable(setter != nullptr)
{
}

Reflection::IProperty::IProperty
(
	Reflection::ValueType type,
	bool canBeSet,
	std::function<Reflection::GenericValue(Reflection::BaseReflectable*)> getter,
	std::function<void(Reflection::BaseReflectable*, Reflection::GenericValue)> setter
) : Type(type), Settable(canBeSet), Get(getter), Set(setter)
{
}

Reflection::IFunction::IFunction
(
	std::vector<ValueType> inputs,
	std::vector<ValueType> outputs,
	std::function<GenericValue(Reflection::BaseReflectable*, GenericValue)> func
) : Inputs(inputs), Outputs(outputs), Func(func)
{
}

Reflection::ReflectionInfo::PropertyMap& Reflection::ReflectionInfo::GetProperties()
{
	return s_Properties;
}

Reflection::ReflectionInfo::FunctionMap& Reflection::ReflectionInfo::GetFunctions()
{
	return s_Functions;
}

bool Reflection::ReflectionInfo::HasProperty(const std::string& name)
{
	if (s_Properties.size() == 0)
		return false;
	return s_Properties.find(name) != s_Properties.end();
}

bool Reflection::ReflectionInfo::HasFunction(const std::string& name)
{
	return s_Functions.find(name) != s_Functions.end();
}

Reflection::IProperty& Reflection::ReflectionInfo::GetProperty(const std::string& name)
{
	if (!this->HasProperty(name))
		throw(std::vformat("(GetProperty) No Property named {}", std::make_format_args(name)));
	else
		return s_Properties.find(name)->second;
}

Reflection::IFunction& Reflection::ReflectionInfo::GetFunction(const std::string& name)
{
	if (!this->HasFunction(name))
		throw(std::vformat("(GetFunction) No Function named {}", std::make_format_args(name)));
	else
		return s_Functions.find(name)->second;
}

Reflection::GenericValue Reflection::ReflectionInfo::GetPropertyValue(const std::string& name)
{
	if (!this->HasProperty(name))
		throw(std::vformat("(GetPropertyValue) No Property named {}", std::make_format_args(name)));
	else
		return s_Properties.find(name)->second.Get((Reflection::BaseReflectable*)this);
}

void Reflection::ReflectionInfo::SetPropertyValue(const std::string& name, GenericValue& value)
{
	if (!this->HasProperty(name))
		throw(std::vformat("(SetPropertyValue) No Property named {}", std::make_format_args(name)));
	else
		s_Properties.find(name)->second.Set((Reflection::BaseReflectable*)this, value);
}

Reflection::GenericValue Reflection::ReflectionInfo::CallFunction(const std::string& name, GenericValue input)
{
	if (!this->HasFunction(name))
		throw(std::vformat("(CallFunction) No Function named {}", std::make_format_args(name)));
	else
		return s_Functions.find(name)->second.Func((Reflection::BaseReflectable*)this, input);
}

Reflection::Reflectable::Reflectable()
{
	Reflectable::ApiReflection = new Reflection::ReflectionInfo();
}

static const char* valueTypeNames[static_cast<int>(Reflection::ValueType::_count)] =
{
		"None",
		"String",
		"Bool",
		"Double",
		"Integer",
		"Color",
		"Vector3",
		"Array",
		"Map"
};

static_assert(
	std::size(valueTypeNames) == (int)Reflection::ValueType::_count,
	"`ValueTypeNames` does not have the same number of elements as `ValueType`"
);

std::string Reflection::TypeAsString(ValueType t)
{
	return valueTypeNames[(int)t];
}
