#pragma once

#include<string>
#include<format>

#include"Reflection.hpp"

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

	Color(Reflection::GenericValue);

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

	operator std::string()
	{
		return std::vformat("{}, {}, {}", std::make_format_args(this->R, this->G, this->B));
	}

	operator Reflection::GenericValue()
	{
		Reflection::GenericValue value;
		value.Type = Reflection::ValueType::Color;

		value.Pointer = malloc(sizeof(Color));
		if (value.Pointer != NULL)
			memcpy(value.Pointer, (void*)this, sizeof(Color));
		else
			throw("Tried to cast Color to Reflection::GenericValue, met an allocation error in the process.");

		return value;
	}
};
