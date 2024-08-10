#include<datatype/Color.hpp>

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

Color::Color(Reflection::GenericValue value)
{
	if (value.Type != Reflection::ValueType::Color)
		throw("GenericValue was not a Color");

	Color* col = (Color*)value.Pointer;

	this->R = col->R;
	this->G = col->G;
	this->B = col->B;
}
