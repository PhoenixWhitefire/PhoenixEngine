#include"datatype/Color.hpp"

Color::Color()
{
	this->R = 0.f;
	this->G = 0.f;
	this->B = 0.f;
}

Color::Color(float R, float G, float B)
{
	this->R = R;
	this->G = G;
	this->B = B;
}

Color::Color(Reflection::GenericValue gv)
	: R(0.f), G(0.f), B(0.f)
{
	if (gv.Type != Reflection::ValueType::Color)
	{
		std::string typeName = Reflection::TypeAsString(gv.Type);
		throw(std::vformat(
			"Attempted to construct Color, but GenericValue was {} instead",
			std::make_format_args(typeName)
		));
	}

	if (!gv.Pointer)
	{
		throw("Attempted to construct Color, but GenericValue.Pointer was NULL");
	}

	Color col = *(Color*)gv.Pointer;
	this->R = col.R;
	this->G = col.G;
	this->B = col.B;
}

Reflection::GenericValue Color::ToGenericValue()
{
	REFLECTION_OPERATORGENERICTOCOMPLEX(Color);
}

std::string Color::ToString()
{
	return std::vformat("{}, {}, {}", std::make_format_args(this->R, this->G, this->B));
}
