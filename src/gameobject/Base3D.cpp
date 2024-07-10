#include"gameobject/Base3D.hpp"

static RenderMaterial* DefaultRenderMat = nullptr;

Object_Base3D::Object_Base3D()
{
	this->Name = "Base3D";
	this->ClassName = "Base3D";

	if (!DefaultRenderMat)
		DefaultRenderMat = RenderMaterial::GetMaterial("err");

	this->Material = DefaultRenderMat;

	m_properties.insert(std::pair(
		"Position",
		PropInfo
		{
		PropType::Vector3,
		PropReflection
		{
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
		}
		}
	));
	m_properties.insert(std::pair(
		"Size",
		PropInfo
		{
		PropType::Vector3,
		PropReflection
		{
			[this]()
			{
				GenericType gt;
				gt.Type = PropType::Vector3;
				gt.Vector3 = this->Size;
				return gt;
			},

			[this](GenericType gt)
			{
				this->Size = gt.Vector3;
			}
		}
		}
	));

	m_properties.insert(std::pair(
		"Material",
		PropInfo
		{
			PropType::String,
			PropReflection
			{
				[this]() { return GenericType{PropType::String, this->Material->Name}; },
				[this](GenericType gt)
				{
					this->Material = RenderMaterial::GetMaterial(gt.String);
				}
			}
		}
	));

	m_properties.insert(std::pair(
		"ColorRGB",
		PropInfo
		{
		PropType::Color,
		PropReflection
		{
			[this]()
			{
				GenericType gt;
				gt.Type = PropType::Color;
				gt.Color3 = this->ColorRGB;
				return gt;
			},

			[this](GenericType gt)
			{
				this->ColorRGB = gt.Color3;
			}
		}
		}
	));
	m_properties.insert(std::pair(
		"Transparency",
		PropInfo
		{
		PropType::Double,
		PropReflection
		{
			[this]()
			{
				GenericType gt;
				gt.Type = PropType::Double;
				gt.Double = this->Transparency;
				return gt;
			},

			[this](GenericType gt)
			{
				this->Transparency = gt.Double;
			}
		}
		}
	));
	m_properties.insert(std::pair(
		"Reflectivity",
		PropInfo
		{
		PropType::Double,
		PropReflection
		{
			[this]()
			{
				GenericType gt;
				gt.Type = PropType::Double;
				gt.Double = this->Reflectivity;
				return gt;
			},

			[this](GenericType gt)
			{
				this->Reflectivity = gt.Double;
			}
		}
		}
	));
	m_properties.insert(std::pair(
		"FaceCulling",
		PropInfo
		{
		PropType::Integer,
		PropReflection
		{
			[this]()
			{
				GenericType gt;
				gt.Type = PropType::Integer;
				gt.Integer = (int)this->FaceCulling;
				return gt;
			},

			[this](GenericType gt)
			{
				this->FaceCulling = (FaceCullingMode)gt.Integer;
			}
		}
		}
	));
}

Mesh* Object_Base3D::GetRenderMesh()
{
	return &this->RenderMesh;
}
