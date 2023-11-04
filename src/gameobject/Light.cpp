#include<gameobject/Light.hpp>

DerivedObjectRegister<Object_PointLight> Object_PointLight::RegisterClassAs("PointLight");
DerivedObjectRegister<Object_SpotLight> Object_SpotLight::RegisterClassAs("SpotLight");
DerivedObjectRegister<Object_DirectionalLight> Object_DirectionalLight::RegisterClassAs("DirectionalLight");
