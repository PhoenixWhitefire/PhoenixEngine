#pragma once

#include<format>
#include<glm/vec3.hpp>

class Vector3
{
public:
	Vector3(glm::vec3 GLMVector);

	Vector3(double X, double Y, double Z);
	Vector3();

	Vector3 Cross(Vector3 OtherVec);
	double Dot(Vector3 OtherVec);

	double X, Y, Z;

	double Magnitude; //Calculated when vector is created (vectors are meant to be immutable)

	static Vector3& ZERO;
	static Vector3& UP;
	static Vector3& DOWN;
	static Vector3& LEFT;
	static Vector3& RIGHT;
	static Vector3& FORWARD;
	static Vector3& BACKWARD;
	static Vector3& ONE;

	Vector3 operator + (Vector3 Other)
	{
		return Vector3(Other.X + this->X, Other.Y + this->Y, Other.Z + this->Z);
	}

	Vector3& operator += (const Vector3 Right)
	{
		Vector3 NewVec = Vector3(X - Right.X, Y - Right.Y, Z - Right.Z);

		this->X = NewVec.X;
		this->Y = NewVec.Y;
		this->Z = NewVec.Z;
		this->Magnitude = NewVec.Magnitude;

		return *this;
	}

	Vector3& operator -= (const Vector3 Right)
	{
		Vector3 NewVec = Vector3(Right.X - this->X, Right.Y - this->Y, Right.Z - this->Z);

		this->X = NewVec.X;
		this->Y = NewVec.Y;
		this->Z = NewVec.Z;
		this->Magnitude = NewVec.Magnitude;

		return *this;
	}

	Vector3 operator * (Vector3 Right)
	{
		return Vector3
		(
			this->X * Right.X,
			this->Y * Right.Y,
			this->Z * Right.Z
		);
	}

	Vector3 operator / (float Divisor)
	{
		return Vector3
		(
			this->X / Divisor,
			this->Y / Divisor,
			this->Z / Divisor
		);
	}

	Vector3 operator * (float Multiplier)
	{
		return Vector3
		(
			this->X * Multiplier,
			this->Y * Multiplier,
			this->Z * Multiplier
		);
	}

	Vector3 operator / (double Divisor)
	{
		return Vector3
		(
			this->X / Divisor,
			this->Y / Divisor,
			this->Z / Divisor
		);
	}

	Vector3 operator * (double Multiplier)
	{
		return Vector3
		(
			this->X * Multiplier,
			this->Y * Multiplier,
			this->Z * Multiplier
		);
	}

	Vector3 operator - (Vector3 Other) 
	{
		return Vector3(this->X - Other.X, this->Y - Other.Y, this->Z - Other.Z);
	}

	bool operator == (Vector3 Other)
	{
		if (Other.X == this->X && Other.Y == this->Y && Other.Z == this->Z)
			return true;
		else
			return false;
	}

	bool operator != (Vector3 Other)
	{
		return !this->operator==(Other);
	}

	operator glm::vec3()
	{
		return glm::vec3(this->X, this->Y, this->Z);
	}

	operator std::string()
	{
		return std::vformat("{}, {}, {}", std::make_format_args(this->X, this->Y, this->Z));
	}
};
