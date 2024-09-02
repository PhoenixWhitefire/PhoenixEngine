#include<glm/geometric.hpp>

#include"datatype/Vector3.hpp"

Vector3 Vector3::zero = Vector3(0.f, 0.f, 0.f);
Vector3 Vector3::xAxis = Vector3(-1.f, 0.f, 0.f);
Vector3 Vector3::yAxis = Vector3(0.f, 1.f, 0.f);
Vector3 Vector3::zAxis = Vector3(0.f, 0.f, 1.f);
Vector3 Vector3::one = Vector3(1.f, 1.f, 1.f);

static bool s_DidInitReflection = false;

struct IVector3
{
	double X = 0.f, Y = 0.f, Z = 0.f;
};

void Vector3::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	//Vector3::ApiReflection = new Reflection::ReflectionInfo();
	/*
	REFLECTION_DECLAREPROP_SIMPLE_READONLY(Vector3, X, Double);
	REFLECTION_DECLAREPROP_SIMPLE_READONLY(Vector3, Y, Double);
	REFLECTION_DECLAREPROP_SIMPLE_READONLY(Vector3, Z, Double);
	REFLECTION_DECLAREPROP_SIMPLE_READONLY(Vector3, Magnitude, Double);

	std::vector<Reflection::ValueType> ins = { Reflection::ValueType::Vector3, Reflection::ValueType::Bool };

	REFLECTION_DECLAREFUNC(
		"FuzzyEq",
		ins,
		{ Reflection::ValueType::Bool },
		[](Reflection::Reflectable* p, Reflection::GenericValue& gv)
		{
			Vector3 me = *dynamic_cast<Vector3*>(p);
			Vector3 other = Vector3(gv.Array[0]);
			double fuzz = gv.Array[1].AsDouble();

			double totalDiff = std::abs(me.X - other.X) + std::abs(me.Y - other.Y) + std::abs(me.Z - other.Z);

			return Reflection::GenericValue(totalDiff <= fuzz);
		}
	);*/

	//REFLECTION_INHERITAPI(Reflection::Reflectable);
}

Vector3::Vector3()
{
	this->X = 0.f;
	this->Y = 0.f;
	this->Z = 0.f;

	this->Magnitude = 0.f;

	s_DeclareReflections();
}

Vector3::Vector3(double x, double y, double z)
{
	this->X = x;
	this->Y = y;
	this->Z = z;

	this->Magnitude = sqrt((this->X * this->X) + (this->Y * this->Y) + (this->Z * this->Z));

	s_DeclareReflections();
}

Vector3::Vector3(glm::tvec3<double, glm::highp> GLMVector)
{
	this->X = GLMVector.x;
	this->Y = GLMVector.y;
	this->Z = GLMVector.z;

	this->Magnitude = sqrt((this->X * this->X) + (this->Y * this->Y) + (this->Z * this->Z));

	s_DeclareReflections();
}

Vector3::Vector3(glm::vec3 GLMVector)
{
	this->X = GLMVector.x;
	this->Y = GLMVector.y;
	this->Z = GLMVector.z;

	this->Magnitude = sqrt((this->X * this->X) + (this->Y * this->Y) + (this->Z * this->Z));

	s_DeclareReflections();
}

Vector3::Vector3(Reflection::GenericValue gv)
	: X(0.f), Y(0.f), Z(0.f), Magnitude(0.f)
{
	if (gv.Type != Reflection::ValueType::Vector3)
	{
		std::string typeName = Reflection::TypeAsString(gv.Type);
		throw(std::vformat(
			"Attempted to construct Vector3, but GenericValue was {} instead",
			std::make_format_args(typeName)
		));
	}

	if (!gv.Pointer)
		throw("Attempted to construct Vector3, but GenericValue.Pointer was NULL");

	/*Vector3 vec = *(Vector3*)gv.Pointer;
	this->X = vec.X;
	this->Y = vec.Y;
	this->Z = vec.Z;
	this->Magnitude = vec.Magnitude;*/

	IVector3* simplevec = static_cast<IVector3*>(gv.Pointer);

	Vector3 vec(simplevec->X, simplevec->Y, simplevec->Z);

	this->X = vec.X;
	this->Y = vec.Y;
	this->Z = vec.Z;
	this->Magnitude = vec.Magnitude;

	s_DeclareReflections();
}

Reflection::GenericValue Vector3::ToGenericValue()
{
	//REFLECTION_OPERATORGENERICTOCOMPLEX(Vector3);

	IVector3 simplevec{ X, Y, Z };

	Reflection::GenericValue gv;
	gv.Type = Reflection::ValueType::Vector3;
	gv.Pointer = malloc(sizeof(IVector3));

	if (!gv.Pointer)
		throw("Allocation error on Vector3::ToGenericValue");

	memcpy(gv.Pointer, &simplevec, sizeof(simplevec));

	return gv;
}

std::string Vector3::ToString()
{
	return std::vformat("{}, {}, {}", std::make_format_args(X, Y, Z));
}

double Vector3::Dot(Vector3 OtherVec)
{
	/*double Product = 0.f;

	Product += this->X * OtherVec.X;
	Product += this->Y * OtherVec.Y;
	Product += this->Z * OtherVec.Z;

	return Product;*/

	// Gave up
	auto me = glm::tvec3<double, glm::highp>(*this);
	auto them = glm::tvec3<double, glm::highp>(OtherVec);
	return glm::dot(me, them);
}

Vector3 Vector3::Cross(Vector3 OtherVec)
{
	/*Vector3 CrossVec;

	CrossVec.X = this->Y * OtherVec.Z - this->Z * OtherVec.Y;
	CrossVec.Y = this->Z * OtherVec.X - this->X * OtherVec.Z;
	CrossVec.Z = this->X * OtherVec.Y - this->Y * OtherVec.X;

	return CrossVec;*/

	// Gave up
	auto me = glm::tvec3<double, glm::highp>(*this);
	auto them = glm::tvec3<double, glm::highp>(OtherVec);
	return glm::cross(me, them);
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
	this->Magnitude = NewVec.Magnitude;

	return *this;
}

Vector3& Vector3::operator-=(const Vector3 Right)
{
	Vector3 NewVec = Vector3(Right.X - this->X, Right.Y - this->Y, Right.Z - this->Z);

	this->X = NewVec.X;
	this->Y = NewVec.Y;
	this->Z = NewVec.Z;
	this->Magnitude = NewVec.Magnitude;

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

Vector3::operator glm::tvec3<float, glm::highp>()
{
	return glm::vec3(this->X, this->Y, this->Z);
}

Vector3::operator glm::tvec3<double, glm::highp>()
{
	return glm::vec3(this->X, this->Y, this->Z);
}