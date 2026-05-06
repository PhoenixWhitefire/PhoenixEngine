// Ref.hpp, Compile times are too long
#pragma once
#include <cstdint>

class GameObject;

// `GameObject*` directly has the least overhead. `ObjectRef`, a little more,
// and `ObjectHandle`, more than that.
// Use `GameObject*` when you know the World Array won't get re-allocated
// for the scope of your variable.
// Use `ObjectRef` when you know nothing will try to delete your Object.
// Use `ObjectHandle` when you can't trust anybody.
// 20/09/2025

// `ObjectRef`: a weak reference to a GameObject
struct ObjectRef
{
	ObjectRef() = default;
	ObjectRef(const GameObject* Object);

	GameObject* Referred() const;
	bool IsValid() const;

	bool operator == (const ObjectRef& them) const;
	GameObject* operator -> () const;
	operator GameObject* () const;
	operator bool () const;

	uint32_t TargetId = UINT32_MAX;
};

// `ObjectHandle`: a strong reference to a GameObject, prevents it from being de-alloc'd
struct ObjectHandle
{
	ObjectRef Reference;

	ObjectHandle() = default;
	ObjectHandle(GameObject* Object);
	ObjectHandle(const ObjectRef& Other);
	ObjectHandle(const ObjectHandle& Other);
	ObjectHandle(ObjectHandle&& Other);

	~ObjectHandle();

	bool HasValue() const;
	GameObject* Dereference() const;
	void Clear();

	ObjectHandle& operator = (const ObjectHandle& them);
	bool operator == (const ObjectHandle& them) const;
	GameObject* operator -> () const;
	operator bool () const;
};
