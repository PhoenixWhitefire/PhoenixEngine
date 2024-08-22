#pragma once

#include"datatype/GameObject.hpp"

struct MapLoader
{
	static void LoadMapIntoObject(const std::string& MapFilePath, GameObject* MapParent);
};
