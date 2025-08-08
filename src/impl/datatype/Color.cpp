#include <format>

#include "datatype/Color.hpp"
#include "Utilities.hpp"

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

Color::Color(const Reflection::GenericValue& gv)
	: R(0.f), G(0.f), B(0.f)
{
	if (gv.Type != Reflection::ValueType::Color)
		RAISE_RT(std::format(
			"Attempted to construct Color, but GenericValue was a {} instead",
			Reflection::TypeAsString(gv.Type)
		));
		
	this->R = gv.Val.Vec3.x;
	this->G = gv.Val.Vec3.y;
	this->B = gv.Val.Vec3.z;
}

Reflection::GenericValue Color::ToGenericValue() const
{
	Reflection::GenericValue gv;
	gv.Type = Reflection::ValueType::Color;
	gv.Val.Vec3 = { R, G, B };

	return gv;
}

std::string Color::ToString()
{
	return std::format("{}, {}, {}", this->R, this->G, this->B);
}
