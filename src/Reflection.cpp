#include<format>

#include"Reflection.hpp"
#include"datatype/Vector3.hpp"
#include"datatype/Color.hpp"

Reflection::GenericValue::GenericValue()
{
	this->Type = ValueType::NONE;
}

Reflection::GenericValue::GenericValue(std::string str)
{
	this->Type = ValueType::String;
	this->String = str;
}

Reflection::GenericValue::GenericValue(bool b)
{
	this->Type = ValueType::Bool;
	this->Bool = b;
}

Reflection::GenericValue::GenericValue(double d)
{
	this->Type = ValueType::Double;
	this->Double = d;
}

Reflection::GenericValue::GenericValue(int i)
{
	this->Type = ValueType::Integer;
	this->Integer = i;
}

Reflection::GenericValue::GenericValue(uint32_t i)
{
	this->Type = ValueType::Integer;
	this->Integer = i;
}

std::string Reflection::GenericValue::ToString()
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
		return std::string(*(Color*)this->Pointer);

	case (ValueType::Vector3):
		return std::string(*(Vector3*)this->Pointer);

	default:
	{
		int tInt = (int)this->Type;
		return std::vformat("GenericValue::ToString failed, Type ID was: {}", std::make_format_args(tInt));
	}
	}
}

Reflection::Property::Property
(
	Reflection::ValueType type,
	std::function<GenericValue(void)> getter,
	std::function<void(GenericValue)> setter
)
{
	this->Type = type;
	this->Getter = getter;
	this->Setter = setter;
}

Reflection::Function::Function
(
	std::vector<ValueType> inputs,
	std::vector<ValueType> outputs,
	std::function<GenericValueArray(GenericValueArray)> func
)
{
	this->Inputs = inputs;
	this->Outputs = outputs;
	this->Func = func;
}

Reflection::PropertyMap& Reflection::Reflectable::GetProperties()
{
	return this->m_properties;
}

Reflection::FunctionMap& Reflection::Reflectable::GetFunctions()
{
	return this->m_functions;
}

bool Reflection::Reflectable::HasProperty(const std::string& name)
{
	return this->m_properties.find(name) != this->m_properties.end();
}

bool Reflection::Reflectable::HasFunction(const std::string& name)
{
	return this->m_functions.find(name) != this->m_functions.end();
}

Reflection::Property& Reflection::Reflectable::GetProperty(const std::string& name)
{
	if (!this->HasProperty(name))
		throw(std::vformat("(GetProperty) No Property named {}", std::make_format_args(name)));
	else
		return this->m_properties.find(name)->second;
}

Reflection::Function& Reflection::Reflectable::GetFunction(const std::string& name)
{
	if (!this->HasFunction(name))
		throw(std::vformat("(GetFunction) No Function named {}", std::make_format_args(name)));
	else
		return this->m_functions.find(name)->second;
}

Reflection::GenericValue Reflection::Reflectable::GetPropertyValue(const std::string& name)
{
	if (!this->HasProperty(name))
		throw(std::vformat("(GetPropertyValue) No Property named {}", std::make_format_args(name)));
	else
		return this->m_properties.find(name)->second.Getter();
}

void Reflection::Reflectable::SetPropertyValue(const std::string& name, GenericValue& value)
{
	if (!this->HasProperty(name))
		throw(std::vformat("(SetPropertyValue) No Property named {}", std::make_format_args(name)));
	else
		this->m_properties.find(name)->second.Setter(value);
}

Reflection::GenericValueArray Reflection::Reflectable::CallFunction(const std::string& name, GenericValueArray input)
{
	if (!this->HasFunction(name))
		throw(std::vformat("(CallFunction) No Function named {}", std::make_format_args(name)));
	else
		return this->m_functions.find(name)->second(input);
}
