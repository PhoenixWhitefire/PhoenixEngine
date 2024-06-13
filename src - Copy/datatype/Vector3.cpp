#include<datatype/Vector3.hpp>
#include"glm/geometric.hpp"

Vector3& Vector3::ZERO = *new Vector3(0.f, 0.f, 0.f);
Vector3& Vector3::UP = *new Vector3(0.f, 1.f, 0.f);
Vector3& Vector3::DOWN = *new Vector3(0.f, -1.f, 0.f);
Vector3& Vector3::LEFT = *new Vector3(-1.f, 0.f, 0.f);
Vector3& Vector3::RIGHT = *new Vector3(1.f, 0.f, 0.f);
Vector3& Vector3::FORWARD = *new Vector3(0.f, 0.f, -1.f);
Vector3& Vector3::BACKWARD = *new Vector3(0.f, 0.f, 1.f);
Vector3& Vector3::ONE = *new Vector3(1.f, 1.f, 1.f);

Vector3::Vector3(glm::vec3 GLMVector)
{
	this->X = GLMVector.x;
	this->Y = GLMVector.y;
	this->Z = GLMVector.z;

	this->Magnitude = sqrt((this->X * this->X) + (this->Y * this->Y) + (this->Z * this->Z));
}

Vector3::Vector3(double X, double Y, double Z) {
	this->X = X;
	this->Y = Y;
	this->Z = Z;

	this->Magnitude = sqrt((this->X * this->X) + (this->Y * this->Y) + (this->Z * this->Z));
}

Vector3::Vector3() {
	this->X = 0.f;
	this->Y = 0.f;
	this->Z = 0.f;

	this->Magnitude = 0.f;
}

double Vector3::Dot(Vector3 OtherVec)
{
	/*double Product = 0.f;

	Product += this->X * OtherVec.X;
	Product += this->Y * OtherVec.Y;
	Product += this->Z * OtherVec.Z;

	return Product;*/

	// Gave up
	return glm::dot(glm::vec3(*this), glm::vec3(OtherVec));
}

Vector3 Vector3::Cross(Vector3 OtherVec)
{
	/*Vector3 CrossVec;

	CrossVec.X = this->Y * OtherVec.Z - this->Z * OtherVec.Y;
	CrossVec.Y = this->Z * OtherVec.X - this->X * OtherVec.Z;
	CrossVec.Z = this->X * OtherVec.Y - this->Y * OtherVec.X;

	return CrossVec;*/

	// Gave up
	return glm::cross(glm::vec3(*this), glm::vec3(OtherVec));
}
