#include"gameobject/Model.hpp"

DerivedObjectRegister<Object_Model> Object_Model::RegisterClassAs("Model");

Object_Model::Object_Model()
{
	this->Name = "Model";
	this->ClassName = "Model";

	m_properties.insert(std::pair(
		"Position",
		std::pair(PropType::Vector3, std::pair(
			[this]()
			{
				GenericType gt;
				gt.Type = PropType::Vector3;
				gt.Vector3 = Vector3((glm::vec3)this->Matrix[3]);
				return gt;
			},

			[this](GenericType gt)
			{
				this->Matrix[3] = glm::vec4(gt.Vector3.X, gt.Vector3.Y, gt.Vector3.Z, 1.f);
			}
		))
	));
}
