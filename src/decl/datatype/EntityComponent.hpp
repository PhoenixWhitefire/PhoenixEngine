#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

enum class EntityComponent : uint8_t
{
	None,

	Transform,
	Mesh,
	ParticleEmitter,
	Sound,
	Workspace,
	DataModel,
	PointLight,
	DirectionalLight,
	SpotLight,
	Camera,
	Animation,
	Model,
	Bone,
	TreeLink,
	Engine,
	PlayerInput,
	AssetManager,
	History,
	RigidBody,
	PhysicsService,
	Renderer,
	Collections,

	__count
};

static inline const std::string_view s_EntityComponentNames[] = {
	"<NONE>",
	"Transform",
	"Mesh",
	"ParticleEmitter",
	"Sound",
	"Workspace",
	"DataModel",
	"PointLight",
	"DirectionalLight",
	"SpotLight",
	"Camera",
	"Animation",
	"Model",
	"Bone",
	"TreeLink",
	"Engine",
	"PlayerInput",
	"AssetManager",
	"History",
	"RigidBody",
	"Physics",
	"Renderer",
	"Collections"
};

static_assert(std::size(s_EntityComponentNames) == (size_t)EntityComponent::__count);

const std::string_view s_DataModelServices[] = {
    "Workspace",
    "Engine",
    "PlayerInput",
    "AssetManager",
    "History",
	"Physics",
	"Renderer",
	"Collections"
};

// component type and ID
// if type is "None", the ID is a Game Object ID
struct ReflectorRef
{
	void* Referred() const;

	template <class T>
	T* Get()
	{
		assert(T::Type == Type);
		return (T*)Referred();
	}

	uint32_t Id = UINT32_MAX;
	EntityComponent Type = EntityComponent::None;

	bool operator == (const ReflectorRef& Other) const
	{
		return Type == Other.Type && Id == Other.Id;
	}

	bool operator != (const ReflectorRef& Other) const
	{
		return !(*this == Other);
	}
};

namespace std
{
	template <>
	struct hash<ReflectorRef>
	{
		size_t operator()(const ReflectorRef& r) const noexcept
		{
			return ((size_t)r.Id << 32) + (size_t)r.Type;
		};
	};
}

extern const std::unordered_map<std::string_view, EntityComponent> s_ComponentNameToType;
EntityComponent FindComponentTypeByName(const std::string_view&);
