// 25/10/2024
// a `GameObject` with a fixed `Transform` from it's Parent
// (i.e. it moves along with it's parent)

#include "gameobject/Attachment.hpp"
#include "gameobject/Base3D.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Attachment);

static bool s_DidInitReflection = false;

// ascend up the `Attachment`'s Parent chain, and get the `.Transform` of the first
// `Base3D`, transformed by any intervening `Attachment`s
static glm::mat4 getNearestParentTransform(Object_Attachment* att)
{
	glm::mat4 trans{ 1.f };
	GameObject* curParent = att->GetParent();

	while (curParent)
	{
		Object_Attachment* parentAtt = dynamic_cast<Object_Attachment*>(curParent);
		if (parentAtt)
			trans *= parentAtt->LocalTransform;
		else
		{
			Object_Base3D* transformableParent = dynamic_cast<Object_Base3D*>(curParent);
			if (transformableParent)
			{
				trans *= transformableParent->Transform;
				break;
			}
			else
				curParent = curParent->GetParent();
		}
	}

	return trans;
}

void Object_Attachment::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);

	REFLECTION_DECLAREPROP_SIMPLE(Object_Attachment, LocalTransform, Matrix);
	REFLECTION_DECLAREPROP(
		"WorldTransform",
		Matrix,
		[](Reflection::Reflectable* p)
		{
			return dynamic_cast<Object_Attachment*>(p)->GetWorldTransform();
		},
		nullptr
	);

	/*
	REFLECTION_DECLAREPROP(
		"WorldTransform",
		Matrix,
		[](Reflection::Reflectable* p)
		-> Reflection::GenericValue
		{
			return dynamic_cast<Object_Attachment*>(p)->GetWorldTransform();
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			const glm::mat4& targetWorldTransform = gv.AsMatrix();
			Object_Attachment* att = dynamic_cast<Object_Attachment*>(p);
			
			// TODO 25/10/2024 how to do this
			att->LocalTransform = glm::inverse(targetWorldTransform * att->GetWorldTransform());
		}
	);
	*/
}

Object_Attachment::Object_Attachment()
{
	this->Name = "Attachment";
	this->ClassName = "Attachment";

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

glm::mat4 Object_Attachment::GetWorldTransform()
{
	return getNearestParentTransform(this) * this->LocalTransform;
}
