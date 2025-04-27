#include "datatype/Vector3.hpp"

Vector3 Vector3::zero = Vector3(0.f, 0.f, 0.f);
Vector3 Vector3::xAxis = Vector3(-1.f, 0.f, 0.f);
Vector3 Vector3::yAxis = Vector3(0.f, 1.f, 0.f);
Vector3 Vector3::zAxis = Vector3(0.f, 0.f, 1.f);
Vector3 Vector3::one = Vector3(1.f, 1.f, 1.f);

static bool s_DidInitReflection = false;

Vector3::Vector3()
{
	this->X = 0.f;
	this->Y = 0.f;
	this->Z = 0.f;
}

Vector3::Vector3(double x, double y, double z)
{
	this->X = x;
	this->Y = y;
	this->Z = z;
}

Vector3::Vector3(glm::tvec3<double, glm::highp> GLMVector)
{
	this->X = GLMVector.x;
	this->Y = GLMVector.y;
	this->Z = GLMVector.z;
}

Vector3::Vector3(glm::vec3 GLMVector)
{
	this->X = GLMVector.x;
	this->Y = GLMVector.y;
	this->Z = GLMVector.z;
}

Vector3::Vector3(const Reflection::GenericValue& gv)
{
	if (gv.Type != Reflection::ValueType::Vector3)
	{
		const std::string_view& typeName = Reflection::TypeAsString(gv.Type);
		throw(std::vformat(
			"Attempted to construct Vector3, but GenericValue was {} instead",
			std::make_format_args(typeName)
		));
	}

	if (!gv.Value)
		throw("Attempted to construct Vector3, but GenericValue.Pointer was NULL");

	/*Vector3 vec = *(Vector3*)gv.Pointer;
	this->X = vec.X;
	this->Y = vec.Y;
	this->Z = vec.Z;
	this->Magnitude = vec.Magnitude;*/

	Vector3* gvec = static_cast<Vector3*>(gv.Value);

	Vector3 vec(gvec->X, gvec->Y, gvec->Z);

	this->X = vec.X;
	this->Y = vec.Y;
	this->Z = vec.Z;
}

Reflection::GenericValue Vector3::ToGenericValue() const
{
	//REFLECTION_OPERATORGENERICTOCOMPLEX(Vector3);

	Reflection::GenericValue gv;
	gv.Type = Reflection::ValueType::Vector3;
	gv.Value = new Vector3(*this);

	return gv;
}

std::string Vector3::ToString()
{
	return std::vformat("{}, {}, {}", std::make_format_args(X, Y, Z));
}

Vector3 Vector3::Cross(Vector3 OtherVec) const
{
	/*Vector3 CrossVec;

	CrossVec.X = this->Y * OtherVec.Z - this->Z * OtherVec.Y;
	CrossVec.Y = this->Z * OtherVec.X - this->X * OtherVec.Z;
	CrossVec.Z = this->X * OtherVec.Y - this->Y * OtherVec.X;

	return CrossVec;*/

	// Gave up
	auto me = glm::tvec3<double, glm::highp>(Vector3(*this));
	auto them = glm::tvec3<double, glm::highp>(OtherVec);
	return glm::cross(me, them);
}

double Vector3::Dot(Vector3 OtherVec) const
{
	/*double Product = 0.f;

	Product += this->X * OtherVec.X;
	Product += this->Y * OtherVec.Y;
	Product += this->Z * OtherVec.Z;

	return Product;*/

	// Gave up
	auto me = glm::tvec3<double, glm::highp>(Vector3(*this));
	auto them = glm::tvec3<double, glm::highp>(OtherVec);
	return glm::dot(me, them);
}

double Vector3::Magnitude() const
{
	return sqrt((this->X * this->X) + (this->Y * this->Y) + (this->Z * this->Z));
}

Vector3 Vector3::Abs() const
{
	return Vector3(std::abs(X), std::abs(Y), std::abs(Z));
}

Vector3 Vector3::operator+(Vector3 Other)
{
	return Vector3(Other.X + this->X, Other.Y + this->Y, Other.Z + this->Z);
}

Vector3& Vector3::operator+=(const Vector3 Right)
{
	Vector3 NewVec = Vector3(X + Right.X, Y + Right.Y, Z + Right.Z);

	this->X = NewVec.X;
	this->Y = NewVec.Y;
	this->Z = NewVec.Z;

	return *this;
}

Vector3& Vector3::operator-=(const Vector3 Right)
{
	Vector3 NewVec = Vector3(Right.X - this->X, Right.Y - this->Y, Right.Z - this->Z);

	this->X = NewVec.X;
	this->Y = NewVec.Y;
	this->Z = NewVec.Z;

	return *this;
}

Vector3 Vector3::operator*(Vector3 Right)
{
	return Vector3
	(
		this->X * Right.X,
		this->Y * Right.Y,
		this->Z * Right.Z
	);
}

Vector3 Vector3::operator/(float Divisor)
{
	return Vector3
	(
		this->X / Divisor,
		this->Y / Divisor,
		this->Z / Divisor
	);
}

Vector3 Vector3::operator*(float Multiplier)
{
	return Vector3
	(
		this->X * Multiplier,
		this->Y * Multiplier,
		this->Z * Multiplier
	);
}

Vector3 Vector3::operator/(double Divisor)
{
	return Vector3
	(
		this->X / Divisor,
		this->Y / Divisor,
		this->Z / Divisor
	);
}

Vector3 Vector3::operator*(double Multiplier)
{
	return Vector3
	(
		this->X * Multiplier,
		this->Y * Multiplier,
		this->Z * Multiplier
	);
}

Vector3 Vector3::operator-(Vector3 Other)
{
	return Vector3(this->X - Other.X, this->Y - Other.Y, this->Z - Other.Z);
}

Vector3 Vector3::operator-()
{
	return Vector3(-X, -Y, -Z);
}

bool Vector3::operator==(Vector3 Other)
{
	if (Other.X == this->X && Other.Y == this->Y && Other.Z == this->Z)
		return true;
	else
		return false;
}

Vector3::operator glm::tvec3<float, glm::highp>() const
{
	return glm::vec3(this->X, this->Y, this->Z);
}

Vector3::operator glm::tvec3<double, glm::highp>() const
{
	return glm::vec3(this->X, this->Y, this->Z);
}