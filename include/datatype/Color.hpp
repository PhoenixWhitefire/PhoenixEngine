#pragma once

class Color
{
public:
	float R, G, B;

	/**
	Create a color from a range of 0 to 1
	@param The R, G and B values of the color
	@return A color made from the specified RGB values
	*/
	Color(float R, float G, float B);

	/**
	Create a blank (black) color
	@return The color black
	*/
	Color();
};
