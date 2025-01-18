#pragma once

#include <string>

#include "Reflection.hpp"

class Color
{
public:

	/**
	Create a blank (black) color
	@return The color black
	*/
	Color();

	/**
	Create a color from a range of 0 to 1
	@param The R, G and B values of the color
	@return A color made from the specified RGB values
	*/
	Color(float R, float G, float B);
	Color(const Reflection::GenericValue&);

	Reflection::GenericValue ToGenericValue() const;
	// Returns formatted `R, G, B`
	std::string ToString();

	float R, G, B;

	Color operator*(Color other)
	{
		return Color(
			this->R * other.R,
			this->G * other.G,
			this->B * other.B
		);
	}

	Color operator*(float other)
	{
		return Color(
			this->R * other,
			this->G * other,
			this->B * other
		);
	}
};
