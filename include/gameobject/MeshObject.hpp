#pragma once

#include<datatype/Color.hpp>
#include<datatype/Mesh.hpp>

#include<datatype/GameObject.hpp>
#include<gameobject/Base3D.hpp>

class Object_Mesh : public Object_Base3D
{
public:
	std::string Name = "MeshPart";
	std::string ClassName = "MeshPart";

	std::vector<Texture*> Textures;

	bool HasTransparency = false;

private:
	std::vector<Vertex> BlankVertices;
	std::vector<GLuint> BlankIndices;

	static DerivedObjectRegister<Object_Mesh> RegisterClassAs;
};