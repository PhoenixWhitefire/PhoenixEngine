#pragma once

#include <format>
#include <glm/vec3.hpp>

#include "Reflection.hpp"

class Vector3 : public Reflection::Reflectable
{
public:
	Vector3();

	Vector3(double X, double Y, double Z);
	Vector3(glm::tvec3<double, glm::highp>);
	Vector3(glm::vec3);
	Vector3(const Reflection::GenericValue&);

	Reflection::GenericValue ToGenericValue() const;
	// Returns formatted `X, Y, Z`
	std::string ToString();

	Vector3 Cross(Vector3 OtherVec) const;
	double Dot(Vector3 OtherVec) const;
	double Magnitude() const;
	Vector3 Abs() const;

	double X, Y, Z;

	static Vector3 zero;
	static Vector3 xAxis;
	static Vector3 yAxis;
	static Vector3 zAxis;
	static Vector3 one;

	Vector3 operator + (Vector3 Other);
	Vector3& operator += (const Vector3 Right);
	Vector3& operator -= (const Vector3 Right);
	Vector3 operator * (Vector3 Right);
	Vector3 operator / (float Divisor);
	Vector3 operator * (float Multiplier);
	Vector3 operator / (double Divisor);
	Vector3 operator * (double Multiplier);
	Vector3 operator - (Vector3 Other);
	Vector3 operator - ();
	bool operator == (Vector3 Other);
	operator glm::tvec3<double, glm::highp>() const;
	operator glm::tvec3<float, glm::highp>() const;

private:
	static void s_DeclareReflections();
};
