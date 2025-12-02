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

		lua_Debug callerAr{};
		lua_getinfo(L, 1, "sln", &callerAr);
		std::string callerInfo = std::format(
			"{}:{} in {}",
			callerAr.short_src,
			callerAr.currentline,
			callerAr.name ? callerAr.name : "<anonymous>"
		);

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
		lua_State* eL = lua_newthread(L);
		lua_State* cL = lua_newthread(eL);
		int threadRef = lua_ref(L, -1);
		lua_xpush(L, eL, 2);
		luaL_sandboxthread(cL);

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

		uint32_t cnId = rev->Connect(
			ev->Reflector.Referred(),

			[eL, cL, rev, callerInfo](const std::vector<Reflection::GenericValue>& Inputs)
			-> void
			{
				ZoneScopedN("RunEventCallback");
				ZoneText(callerInfo.data(), callerInfo.size());

				assert(Inputs.size() == rev->CallbackInputs.size());
				assert(lua_isfunction(eL, 2));

				lua_pushlightuserdata(eL, eL);
				lua_gettable(eL, LUA_ENVIRONINDEX);
				EventConnectionData* cn = (EventConnectionData*)luaL_checkudata(eL, -1, "EventConnection");
				lua_pop(eL, 1);

				if (cn->Script.HasValue())
				{
					GameObject* scr = (GameObject*)cn->Script.Dereference();

					if (!scr || !scr->FindComponentByType(EntityComponent::Script))
					{
						cn->Event->Disconnect(cn->Reflector.Referred(), cn->ConnectionId);
						lua_resetthread(cL);
						lua_resetthread(eL);

						return;
					}
				}
				lua_State* co = cL;

				// performance optimization:
				// if the callback never yields, then we know that
				// there cannot be more than one instance of it
				// running concurrently. thus, we can re-use a single thread
				// instead of creating a new one for each invocation
				if (cn->CallbackYields)
				{
					lua_State* nL = lua_newthread(eL);
					luaL_checkstack(nL, (int32_t)Inputs.size() + 1, "event connection callback args");
					lua_xpush(eL, nL, 2);
					lua_pop(eL, 1); // pop nL off eL

					luaL_sandboxthread(nL);
					co = nL;
				}
				else
					lua_xpush(eL, cL, 2);

				for (size_t i = 0; i < Inputs.size(); i++)
				{
					assert(Inputs[i].Type == rev->CallbackInputs[i]);
					ScriptEngine::L::PushGenericValue(co, Inputs[i]);
				}

				ZoneNamedN(pcallzone, "Do PCall", true);

				// TODO 11/08/2025
				// they added a "correct" way to do this, with continuations n stuff
				// but i genuinely just could not be bothered
				co->baseCcalls++;
				int status = lua_pcall(co, static_cast<int32_t>(Inputs.size()), 0, 0);
				co->baseCcalls--;
				// TODO 06/09/2025
				// i dont even know
				// is this intentional??
				// only a problem when the status is YIELD or BREAK,
				// in which case `lua_pcall` misleading returns `0`
				// other times it doesnt matter
				status = status == 0 ? co->status : status;

				if (status != LUA_OK && status != LUA_YIELD && status != LUA_BREAK)
				{
					luaL_checkstack(co, 2, "error string"); // tolstring may push a metatable temporarily as well
					Log::ErrorF("Script event: {}", luaL_tolstring(co, -1, nullptr));
					ScriptEngine::L::DumpStacktrace(co);
					lua_pop(co, 2);
				}

				if (status == LUA_BREAK)
				{
					cn->CallbackYields = true;
					while (co->status == LUA_BREAK)
						lua_resume(co, nullptr, 0); // i dont knowwwwww
				}

				if (!cn->CallbackYields && status == LUA_YIELD)
					cn->CallbackYields = true;

				if (status == LUA_OK && cn->CallbackYields && cL->status == LUA_OK)
				{ // unmark the callback as yielding if we didnt yield and the original thread is ready to be re-used
					lua_pop(cL, lua_gettop(cL)); // not entirely sure how these are piling up
					cn->CallbackYields = false;
				}
			}
		);

		ec->Reflector = ev->Reflector;
		ec->SignalRef = signalRef;
		ec->ThreadRef = threadRef;
		ec->ConnectionId = cnId;
		ec->Event = rev;
		ec->L = L;

		lua_getglobal(L, "script");
		Reflection::GenericValue scrgv = ScriptEngine::L::ToGeneric(L, -1);
		if (scrgv.Type == Reflection::ValueType::GameObject)
		{
			GameObject* scr = GameObject::FromGenericValue(scrgv);
			ec->Script = scr;
		}

		lua_pop(L, 1);

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

			[ev, resume, reflector, rev, values](const std::vector<Reflection::GenericValue>& Values)
			-> void
			{
				*values = Values;
				*resume = true;
			}
		);

		return ScriptEngine::L::Yield(
			L,
			rev->CallbackInputs.size(),
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
