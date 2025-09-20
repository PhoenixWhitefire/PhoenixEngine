#pragma once

#include "datatype/GameObject.hpp"

namespace SceneFormat
{
	// Serializes the provided objects to the scene format,
	// load-able through `SceneFormat::FromFile`
	// @param Root objects
	// @param Scene name
	// @return Scene file contents
	std::string Serialize(std::vector<GameObject*>, const std::string&);
	// Return's a vector of the Objects in the scene with their descendants
	// @param Scene file contents
	// @param Pointer a bool value indicating success/failure
	// @param Pointer to a success indicator `bool`. Use`SceneFormat::GetLastError`
	//  to see the error string if this is false
	// @return Root scene contents
	std::vector<ObjectRef> Deserialize(const std::string& Contents, bool*);
	// Gets the last error that occurred.
	std::string GetLastErrorString();
};
