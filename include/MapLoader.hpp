#pragma once

#include<nljson.h>

#include<datatype/GameObject.hpp>
#include<datatype/Material.hpp>
#include<Engine.hpp>
#include"BaseMeshes.hpp"
#include"FileRW.hpp"

class MapLoader
{
public:
	static void LoadMapIntoObject(const char* MapFilePath, std::shared_ptr<GameObject> MapParent);
};
