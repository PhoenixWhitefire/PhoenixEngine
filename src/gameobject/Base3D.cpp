#include<gameobject/Base3D.hpp>

static RenderMaterial* DefaultRenderMat = nullptr;

Object_Base3D::Object_Base3D()
{
	this->Name = "Base3D";
	this->ClassName = "Base3D";

	if (!DefaultRenderMat)
		DefaultRenderMat = new RenderMaterial("floortiles");

	this->Material = DefaultRenderMat;

	m_properties.insert(std::pair(
		std::string("Size"),
		std::pair(PropType::Vector3, std::pair(
			[this]()
			{
				GenericType gt;
				gt.Type = PropType::Vector3;
				gt.Vector3 = this->Size;
				return gt;
			},
			[this](GenericType gt) { this->Size = gt.Vector3; }
		))
	));
	m_properties.insert(std::pair(
		std::string("Position"),
		std::pair(PropType::Vector3, std::pair(
			[this]()
			{
				GenericType gt;
				gt.Type = PropType::Vector3;
				gt.Vector3 = Vector3(glm::vec3(this->Matrix[3]));
				return gt;
			},
			[this](GenericType gt)
			{
				this->Matrix[3] = glm::vec4(gt.Vector3.X, gt.Vector3.Y, gt.Vector3.Z, 1.f);
			}
		))
	));
	m_properties.insert(std::pair(
		std::string("ColorRGB"),
		std::pair(PropType::Color, std::pair(
			[this]()
			{
				GenericType gt;
				gt.Type = PropType::Color;
				gt.Color3 = this->ColorRGB;
				return gt;
			},
			[this](GenericType gt) { this->ColorRGB = gt.Color3; }
		))
	));
	m_properties.insert(std::pair(
		std::string("Transparency"),
		std::pair(PropType::Double, std::pair(
			[this]() { return GenericType{PropType::Double, "", false, this->Transparency}; },
			[this](GenericType gt) { this->Transparency = gt.Double; }
		))
	));
	m_properties.insert(std::pair(
		std::string("Reflectivity"),
		std::pair(PropType::Double, std::pair(
			[this]() { return GenericType{ PropType::Double, "", false, this->Reflectivity }; },
			[this](GenericType gt) { this->Reflectivity = gt.Double; }
		))
	));
}

Mesh* Object_Base3D::GetRenderMesh()
{
	return &this->RenderMesh;
}
