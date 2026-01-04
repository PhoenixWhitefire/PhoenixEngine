#include <tracy/public/tracy/Tracy.hpp>
#include <luau/VM/src/lstate.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"

void luhx_pushsignal(
	lua_State* L,
	const Reflection::EventDescriptor* Event,
	const ReflectorRef& Reflector,
	const char* EventName
)
{
	EventSignalData* ev = (EventSignalData*)lua_newuserdata(L, sizeof(EventSignalData));
	ev->Reflector = Reflector;
	ev->EventName = EventName;
	ev->Event = Event;

	luaL_getmetatable(L, "EventSignal");
	lua_setmetatable(L, -2);
}

static int sig_namecall(lua_State* L)
{
	if (strcmp(L->namecall->data, "Connect") == 0)
	{
		luaL_checktype(L, 2, LUA_TFUNCTION);

		std::string* traceback = new std::string;
		ScriptEngine::L::DumpStacktrace(L, traceback);

		EventSignalData* ev = (EventSignalData*)luaL_checkudata(L, 1, "EventSignal");
		const Reflection::EventDescriptor* rev = ev->Event;
		int signalRef = lua_ref(L, 1);

		// TRUST ME, ALL THESE L's MAKE SENSE OK
		// `eL` IS. UH. UHHH. *EVENT* L! YES, *E*VENT L
		// THEN, *THEN*, `cL` IS... *C*ONNECTION L!! YES, YES, YES!!!
		// "And then, `nL`?", YOU MAY ASK, (smart and kinda cute as always)
		// ...
		// ...
		// ...
		// ...
		// _*NNNNNNNNNNNNNNN*EW_ L! *NNNNNNN*EW!! *N*EW *N*EW *N*EW *N*EW!!!!!!!
		// MY GENIUS
		// UNPARAARALELLED
		//    ARAARA
		//    ara ara
		// (tee hee)
		lua_State* eL = lua_newthread(lua_mainthread(L));
		luaL_sandboxthread(eL);
		lua_State* cL = lua_newthread(eL);
		int threadRef = lua_ref(lua_mainthread(L), -1); // ref to eL
		lua_xpush(L, eL, 2); // push callback onto eL
		lua_pop(lua_mainthread(L), 1);

		eL->userdata = traceback;

		EventConnectionData* ec = (EventConnectionData*)lua_newuserdata(eL, sizeof(EventConnectionData));
		ec->Script.Reference = nullptr; // zero-initialization breaks some assumptions that IDs which are not `PHX_GAMEOBJECT_NULL_ID` are valid
		luaL_getmetatable(eL, "EventConnection"); // stack: ec, mt
		lua_setmetatable(eL, -2); // stack: ec
		lua_xpush(eL, L, -1);

		// assign the connection to the Event Thread so it can be used by the Connection Threads
		// `eL` itself is never resumed, only `cL` or `nL` (`nL` being created per-invocation if the thread yields every time)
		lua_pushlightuserdata(eL, eL); // stack: ec, lud
		lua_pushvalue(eL, -2); // stack: ec, lud, ec
		lua_settable(eL, LUA_ENVIRONINDEX); // stack: ec
		lua_pop(eL, 1);

		lua_getglobal(L, "script");
		Reflection::GenericValue scrgv = ScriptEngine::L::ToGeneric(L, -1);
		if (scrgv.Type == Reflection::ValueType::GameObject)
		{
			GameObject* scr = GameObject::FromGenericValue(scrgv);
			ec->Script = scr;
		}
		lua_pop(L, 1);

		ObjectHandle scriptRef = ec->Script;
		ReflectorRef reflector = ev->Reflector;

		uint32_t cnId = rev->Connect(
			ev->Reflector.Referred(),

			[eL, ec, cL, rev, scriptRef, reflector, traceback](const std::vector<Reflection::GenericValue>& Inputs, uint32_t ConnectionId) -> void
			{
				ZoneScopedN("RunEventCallback");
				ZoneText(traceback->data(), traceback->size());

				if (scriptRef.HasValue())
				{
					GameObject* scr = scriptRef.Dereference();

					if (!scr || scr->IsDestructionPending || !scr->FindComponentByType(EntityComponent::Script))
					{
						// TODO
						// I'm Glad I finally figured out what was going on, but there's probably a more important
						// architectural problem behind this as well
						// 14/12/2025
						if (reflector.Referred())
							rev->Disconnect(reflector.Referred(), ConnectionId);
						return;
					}
				}

				assert(Inputs.size() == rev->CallbackInputs.size());
				assert(lua_isfunction(eL, 2));

				lua_State* co = cL;

				// performance optimization:
				// if the callback never yields, then we know that
				// there cannot be more than one instance of it
				// running concurrently. thus, we can re-use a single thread
				// instead of creating a new one for each invocation
				if (cL->status == LUA_YIELD)
				{
					lua_State* nL = lua_newthread(eL);
					luaL_sandboxthread(nL);
					co = nL;
				}
				else
					lua_xpush(eL, cL, 2);

				lua_pushthread(co);
				int runnerRef = lua_ref(co, -1);
				lua_pop(co, 1);

				if (co != cL)
					lua_pop(eL, 1);

				ScriptEngine::s_YieldedCoroutines.push_back(ScriptEngine::YieldedCoroutine{
					.DebugString = "DeferredEventResumption",
					.Coroutine = co,
					.CoroutineReference = runnerRef,
					.RmPoll = [rev, Inputs, eL, traceback](lua_State* L) -> int
						{
							lua_resetthread(L);

							assert(lua_isfunction(eL, 2));
							lua_xpush(eL, L, 2);

							for (size_t i = 0; i < Inputs.size(); i++)
							{
								assert(Inputs[i].Type == rev->CallbackInputs[i]);
								ScriptEngine::L::PushGenericValue(L, Inputs[i]);
							}

							L->userdata = new std::string(*traceback);

							return (int)Inputs.size();
						},
					.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Polled
				});
			}
		);

		ec->Reflector = ev->Reflector;
		ec->SignalRef = signalRef;
		ec->ThreadRef = threadRef;
		ec->ConnectionId = cnId;
		ec->Event = rev;
		ec->L = L;

		return 1;
	}
	else if (strcmp(L->namecall->data, "WaitUntil") == 0)
	{
		EventSignalData* ev = (EventSignalData*)luaL_checkudata(L, 1, "EventSignal");
		const Reflection::EventDescriptor* rev = ev->Event;

		ReflectorRef reflector = ev->Reflector;
		bool* resume = new bool;
		std::vector<Reflection::GenericValue>* values = new std::vector<Reflection::GenericValue>;
		*resume = false;

		uint32_t cid = rev->Connect(
			reflector.Referred(),

			[ev, resume, reflector, rev, values](const std::vector<Reflection::GenericValue>& Values, uint32_t)
			-> void
			{
				*values = Values;
				*resume = true;
			}
		);

		return ScriptEngine::L::Yield(
			L,
			(int)rev->CallbackInputs.size(),
			[resume, values, reflector, rev, cid](ScriptEngine::YieldedCoroutine& yc)
			-> void
			{
				yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Polled;
				yc.RmPoll = [resume, values, reflector, rev, cid](lua_State* L) -> int
					{
						if (*resume)
						{
							rev->Disconnect(reflector.Referred(), cid);

							int count = (int)values->size();

							for (const Reflection::GenericValue& gv : *values)
								ScriptEngine::L::PushGenericValue(L, gv);

							delete values;
							delete resume;

							return count;
						}

						return -1;
					};
			}
		);
	}
	else
		luaL_error(L, "No such method of Event Signal known as '%s'", L->namecall->data);
}

static int sig_eq(lua_State* L)
{
    EventSignalData* ev1 = (EventSignalData*)luaL_checkudata(L, 1, "EventSignal");
	EventSignalData* ev2 = (EventSignalData*)luaL_checkudata(L, 2, "EventSignal");

	lua_pushboolean(L, ev1->Event == ev2->Event && ev1->Reflector == ev2->Reflector);
	return 1;
}

static int sig_tostring(lua_State* L)
{
    EventSignalData* ev = (EventSignalData*)luaL_checkudata(L, 1, "EventSignal");
	GameObject* obj = GameObject::GetObjectById(ev->Reflector.Id);

	std::string source = ev->Reflector.Type == EntityComponent::None
		? (obj ? obj->GetFullName() + "." : "GameObject::")
		: std::format("{}::", s_EntityComponentNames[(size_t)ev->Reflector.Type]);

	lua_pushfstring(L, "%s%s", source.c_str(), ev->EventName);
	return 1;
}

static void createmetatable(lua_State* L)
{
    luaL_newmetatable(L, "EventSignal");

    lua_pushstring(L, "EventSignal");
	lua_setfield(L, -2, "__type");

	lua_pushcfunction(L, sig_namecall, "__namecall");
	lua_setfield(L, -2, "__namecall");

	lua_pushcfunction(L, sig_eq, "EventSignal.__eq");
	lua_setfield(L, -2, "EventSignal.__eq");

	lua_pushcfunction(L, sig_tostring, "EventSignal.__tostring");
	lua_setfield(L, -2, "__tostring");

    lua_pop(L, 1);
}

int luhxopen_EventSignal(lua_State* L)
{
    createmetatable(L);
    return 0;
}
