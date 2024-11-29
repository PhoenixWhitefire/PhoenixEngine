// 15/09/2024 PHOENIXWHITEFIRE2000
// A template/example GameObject.

#include "gameobject/Example.hpp"
#include "gameobject/DataModel.hpp"

// This inserts a pointer to the template function `createGameObjectHeir<Object_Example>`
// into the `unordered_map` `GameObject::s_GameObjectMap`, which is then
// called by `GameObject::Create`
PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Example);

static bool s_DidInitReflection = false;

void Object_Example::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	// Inherits the properties of all base classes as well
	REFLECTION_INHERITAPI(Script);

	// We go base-to-derived so members declared in the ancestral classes
	// can be overriden by any inheriting descendants for their own
	// uses

	// Implement our own stuff

	REFLECTION_DECLAREPROP(
		"Value1",
		Bool,
		[](Reflection::Reflectable* p)
		{
			// Implicitly calls the `::GenericValue(bool)` constructor
			// This is not voodoo. It really is returning a `Reflection::GenericValue`
			return dynamic_cast<Object_Example*>(p)->Value1;
		},
		// Making this read-only is as simple as replacing the setter
		// with `nullptr`, which is what `REFLECTION_DECLAREPROP_SIMPLE_READONLY` does
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			// Always use the `::As[X]` methods, even if it's a String for
			// forward-compatibility and built-in sanity-checking
			dynamic_cast<Object_Example*>(p)->Value1 = gv.AsBool();
		}
	);

	// Prop name is `Value2`. It will be read and set exactly like the above `REFLECTION_DECLAREPROP`
	REFLECTION_DECLAREPROP_SIMPLE(Object_Example, Value2, Integer);

	// Can be read, but not set
	REFLECTION_DECLAREPROP_SIMPLE_READONLY(Object_Example, Value4, String);

	// If we used `REFLECTION_DECLAREPROP_SIMPLE`, we'd get a "possible loss of data"
	// warning as we're converting between `double` and `float`
	// Use a `static_cast` to silence the warning
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Example, Value3, Double, float);

	// We can't use `REFLECTION_DECLAREPROP_SIMPLE` here because `GenericValue` doesn't
	// have a constructor accepting `Vector3`, but `Vector3` has a `Reflection::GenericValue`
	// cast operator, which is what `_TYPECAST` calls
	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Example, Value5, Vector3);

	// Functions take in a value, and return another value, possibly generating
	// side-effects. They may not mutate their parameters. The Script engine
	// will enforce the input type on it's side, but, right now (15/09/2024),
	// it does not enforce the output. Really, it only exists for IntelliSense
	// plans for the distant future
	REFLECTION_DECLAREFUNC(
		"Greet",
		{ Reflection::ValueType::Array, Reflection::ValueType::Bool },
		{ Reflection::ValueType::String, Reflection::ValueType::String },
		[](Reflection::Reflectable* r, const std::vector<Reflection::GenericValue>& args)
		-> std::vector<Reflection::GenericValue>
		{
			GameObject* p = dynamic_cast<GameObject*>(r);

			Reflection::GenericValue gv = args.at(0);
			std::vector<Reflection::GenericValue> names = gv.AsArray();

			if (names.empty())
				return { p->Name + ": He- Oh... There's no one here... :(" };

			else if (names.size() == 1)
				return { p->Name + ": Hello, " + names[0].AsString() + "!" };

			// Even though you *could* do
			// 
			// Reflection::GenericValue result = "Hello, ";
			// 
			// and assign to it with
			// 
			// result.String += "XYZ";
			//
			// , it's not very forward-compatible in my opinion.

			std::string result = "Hello, ";

			for (size_t index = 0; index < names.size() - 1; index++)
				result += names[index].AsString() + ", ";

			result = result.substr(0, result.size() - 2);
			result += " and " + names.back().AsString() + "!";

			if (args[1].AsBool())
				result += " (MANIACAL CACKLING) HAHAHAHAHAHAHA";

			std::vector<Reflection::GenericValue> returnVals =
			{
				Reflection::GenericValue(p->Name),
				Reflection::GenericValue(result)
			};

			return returnVals;
		}
	);

	// Procedures are functions that do not return any value. There is no
	// `REFLECTION_DECLAREPROC` because of some macro expansion problems.
	REFLECTION_DECLAREPROC_INPUTLESS(
		GiveUp,
		[](Reflection::Reflectable* p)
		{
			dynamic_cast<GameObject*>(p)->Destroy();

			// It is not enough for just me to cease. Everything else must also.
			if (GameObject::s_DataModel)
				// Coerce. Convince. Force.
				dynamic_cast<Object_DataModel*>(GameObject::s_DataModel)->WantExit = true;

			throw("Gave up. Gave in. Ending it all. The world swallows itself up, or, at the very least, I will no longer exist.");
		}
	);

	// And there you have it! This class will now appear in the `apidump.json`, assuming
	// you've remembered to `#include` it in `src/impl/gameobject/GameObjects.hpp`
}

Object_Example::Object_Example()
{
	this->Name = "Example";
	this->ClassName = "Example";
	
	s_DeclareReflections();
	ApiPointer = &s_Api;

	// Returns whether it succeeded, but this class isn't
	// very important so we don't care
	this->LoadScript("scripts/safe-space.luau");
	// `::LoadScript` won't actually load it if we aren't
	// parented to anything
	this->Reload();
}
