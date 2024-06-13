#include<datatype/Color.hpp>

Color::Color(float R, float G, float B)
{
	this->R = R;
	this->G = G;
	this->B = B;
}

Color::Color()
{
	this->R = 0.f;
	this->G = 0.f;
	this->B = 0.f;
}
