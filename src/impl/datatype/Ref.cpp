// Ref.cpp, Compile times are too long
#include "datatype/Ref.hpp"
#include "datatype/GameObject.hpp"

ObjectRef::ObjectRef(const GameObject* Object)
{
	if (Object)
	{
		if (!Object->Valid)
			RAISE_RT("Tried to create ObjectRef from invalid Object (ID: {}, Path: {})", Object->ObjectId, Object->GetFullName());

		TargetId = Object->ObjectId;
	}
	else
		TargetId = PHX_GAMEOBJECT_NULL_ID;
}

GameObject* ObjectRef::Referred() const
{
	return GameObjectManager::Get()->FindById(TargetId);
}

bool ObjectRef::IsValid() const
{
	return GameObjectManager::Get()->FindById(TargetId) != nullptr;
}

bool ObjectRef::operator==(const ObjectRef& them) const
{
	return TargetId == them.TargetId;
}

GameObject* ObjectRef::operator->() const
{
	GameObject* object = Referred();
	if (!object)
		RAISE_RT("Tried to dereference invalid ObjectRef (Target: {})", TargetId);

	return object;
}

ObjectRef::operator GameObject*() const
{
	return Referred();
}

ObjectRef::operator bool () const
{
	return Referred() != nullptr;
}

ObjectHandle::ObjectHandle(GameObject* Object)
{
	Reference = Object;

	if (Object)
		Object->IncrementHardRefs();
}

static void changeReference(ObjectHandle* that, const ObjectRef& Other)
{
	if (GameObject* prevObj = that->Reference.Referred())
		prevObj->DecrementHardRefs();

	that->Reference = Other;

	if (GameObject* newObject = that->Reference.Referred())
		newObject->IncrementHardRefs();
}

ObjectHandle::ObjectHandle(const ObjectRef& Other)
{
	changeReference(this, Other);
}

ObjectHandle::ObjectHandle(const ObjectHandle& Other)
{
	changeReference(this, Other.Reference);
}

ObjectHandle::ObjectHandle(ObjectHandle&& Other)
{
	changeReference(this, Other.Reference);
	Other.Reference = nullptr;
}

ObjectHandle::~ObjectHandle()
{
	if (!HasValue())
		return;

	if (GameObject* obj = Reference.Referred())
	{
		obj->DecrementHardRefs();
		Reference = nullptr;
	}
}

bool ObjectHandle::HasValue() const
{
	assert(Reference.TargetId > 0);
	return Reference.TargetId != PHX_GAMEOBJECT_NULL_ID;
}

GameObject* ObjectHandle::Dereference() const
{
    if (!HasValue())
        RAISE_RT("Tried to dereference an invalid ObjectHandle (Target: {})", Reference.TargetId);

    GameObject* object = Reference.Referred();
	if (!object)
		RAISE_RT("Dereferenced ObjectHandle had invalid target (ID: {})", Reference.TargetId);

    return object;
}

void ObjectHandle::Clear()
{
	if (HasValue())
		Dereference()->DecrementHardRefs();

	Reference = nullptr;
}

bool ObjectHandle::operator==(const ObjectHandle& them) const
{
	return Reference == them.Reference;
}

ObjectHandle& ObjectHandle::operator=(const ObjectHandle& Other)
{
	changeReference(this, Other.Reference);
	return *this;
}

GameObject* ObjectHandle::operator->() const
{
	return Dereference();
}

ObjectHandle::operator bool () const
{
	return HasValue();
}
