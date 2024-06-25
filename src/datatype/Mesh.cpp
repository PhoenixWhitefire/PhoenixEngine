#include<datatype/Mesh.hpp>

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
	this->Vertices = vertices;
	this->Indices = indices;
}

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, glm::vec3 verticesColor)
{
	this->Vertices = vertices;
	this->Indices = indices;

	for (int Index = 0; Index < vertices.size(); Index++)
		vertices[Index].color = glm::vec3(verticesColor);
}

Mesh::Mesh()
{

}
