#pragma once

#include<datatype/GameObject.hpp>

class MapLoader
{
public:
	static void LoadMapIntoObject(const char* MapFilePath, std::shared_ptr<GameObject> MapParent);
};
