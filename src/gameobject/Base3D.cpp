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
				Vector3 v = Vector3((glm::vec3)this->Matrix[3]);
				return v;
			},

			[this](GenericType gt)
			{

				Vector3& vec = *(Vector3*)gt.Pointer;
				this->Matrix[3] = glm::vec4(vec.X, vec.Y, vec.Z, 1.f);
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
				return this->Size;
			},

			[this](GenericType gt)
			{
				this->Size = gt;
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
				[this]() { return this->Name; },
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
				return this->ColorRGB;
			},

			[this](GenericType gt)
			{
				this->ColorRGB = gt;
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
				return this->Transparency;
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
				return this->Reflectivity;
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
				return (int)this->FaceCulling;
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
