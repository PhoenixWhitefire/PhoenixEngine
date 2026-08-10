#include <tracy/Tracy.hpp>
#include <lualib.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "script/UserdataTags.hpp"
#include "script/LightUserdataTags.hpp"
#include "datatype/ComponentBase.hpp"

void luhx_pushsignal(
    lua_State* L,
    const Reflection::EventDescriptor* Event,
    const ReflectorRef& Reflector,
    const char* EventName,
    uint32_t RestrictToDataModel
)
{
    EventSignalData* ev = (EventSignalData*)lua_newuserdatataggedwithmetatable(L, sizeof(EventSignalData), UserdataTag::EventSignal);
    *ev = {};
    ev->Reflector = Reflector;
    ev->EventName = EventName;
    ev->Event = Event;
    ev->RestrictDataModel = RestrictToDataModel;
}

static void incrementReflectorRefs(ReflectorRef& reflector)
{
    void* referred = reflector.Referred();
    assert(referred);

    if (reflector.Type == EntityComponent::None)
        ((GameObject*)referred)->IncrementHardRefs();
    else
        ((BaseComponent*)referred)->Object->IncrementHardRefs();
}

static void decrementReflectorRefs(ReflectorRef& reflector)
{
    void* referred = reflector.Referred();
    assert(referred);

    if (reflector.Type == EntityComponent::None)
        ((GameObject*)referred)->DecrementHardRefs();
    else
        ((BaseComponent*)referred)->Object->DecrementHardRefs();
}

static void queueEvent(
    lua_State* eL,
    EventConnectionData* ec,
    lua_State* cL,
    const Reflection::EventDescriptor* rev,
    const EventSignalData* ev,
    ObjectRef dmRef,
    ReflectorRef reflector,
    const std::vector<Reflection::GenericValue>&
    Inputs,
    uint32_t ConnectionId,
    uint32_t FromDataModel
)
{
    ZoneScoped;

    if (ec->ConnectionId == UINT32_MAX)
        return; // Event has been disconnected, so our various threads may have been de-allocated

    const std::string& spawnTrace = ((ScriptEngine::L::StateUserdata*)lua_getthreaddata(eL))->SpawnTrace;
    ZoneText(spawnTrace.data(), spawnTrace.size());
    (void)spawnTrace;

    GameObject* dm = dmRef.Referred();

    if (!dm || dm->IsDestructionPending || !dm->FindComponentByType(EntityComponent::DataModel))
    {
        // TODO
        // I'm Glad I finally figured out what was going on, but there's probably a more important
        // architectural problem behind this as well
        // 14/12/2025
        if (reflector.Referred())
            rev->Disconnect(reflector.Referred(), ConnectionId);
        return;
    }

    ScriptEngine::L::StateUserdata* vmud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(eL));

    assert(Inputs.size() == rev->CallbackInputs.size());
    assert(lua_isfunction(eL, 2));

    lua_State* co = cL;

    // performance optimization:
    // if the callback never yields, then we know that
    // there cannot be more than one instance of it
    // running concurrently. thus, we can re-use a single thread
    // instead of creating a new one for each invocation
    if (lua_status(cL) == LUA_YIELD || lua_status(cL) == LUA_ERRRUN)
    {
        lua_State* nL = lua_newthread(eL);
        lua_xpush(eL, nL, 2);
        co = nL;
    }

    lua_pushthread(co);
    int runnerRef = lua_ref(co, -1);
    lua_pop(co, 1);

    if (co != cL)
        lua_pop(eL, 1);

    std::deque<ScriptEngine::YieldedCoroutine>* yieldedCoros = nullptr;

    if (vmud->PVM)
        yieldedCoros = &vmud->PVM->YieldedCoroutinesSync;
    else
        yieldedCoros = &ScriptEngine::VMs.at(vmud->VM).YieldedCoroutines;

    yieldedCoros->push_back(ScriptEngine::YieldedCoroutine{
        .DebugString = "DeferredEventResumption",
        .Coroutine = co,
        .CoroutineReference = runnerRef,
        .DataModel = dmRef,
        .RmEventCallback = {
            .Event = rev,
            .Reflector = reflector,
            .ConnectionId = ec->ConnectionId,
            .RestrictDataModel = ev->RestrictDataModel,
        },
        .RmPoll = [rev, Inputs, eL, ev, FromDataModel](lua_State* L) -> int
            {
                if (ev->RestrictDataModel != UINT32_MAX && FromDataModel != UINT32_MAX)
                {
                    GameObjectManager* objectManager = GameObjectManager::Get();
                    if (objectManager->FindById(ev->RestrictDataModel)->OwningDataModel != objectManager->FindById(FromDataModel)->OwningDataModel)
                        return -1; // not our target dm
                }

                lua_resetthread(L);

                assert(lua_isfunction(eL, 2));
                lua_xpush(eL, L, 2);

                for (size_t i = 0; i < Inputs.size(); i++)
                {
                    assert(Reflection::TypeFits(rev->CallbackInputs[i], Inputs[i].Type));
                    ScriptEngine::L::PushGenericValue(L, Inputs[i]);
                }

                return (int)Inputs.size();
            },
        .Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::DeferredEventResumption
    });
}

static void cleanupConnection(lua_State* L, EventConnectionData* ec)
{
    if (ec->ConnectionId == UINT32_MAX)
        return;

    assert(lua_mainthread(L) == lua_mainthread(ec->L));

    ScriptEngine::L::StateUserdata* ud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(ec->L);
    const auto& it = std::find(ud->EventConnections.begin(), ud->EventConnections.end(), ec);
    assert(it != ud->EventConnections.end());
    ud->EventConnections.erase(it);

    ScriptEngine::L::StateUserdata* vmud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(ec->L));
    std::deque<ScriptEngine::YieldedCoroutine>* yieldedCoros = nullptr;

    if (vmud->PVM)
        yieldedCoros = &vmud->PVM->YieldedCoroutinesSync;
    else
        yieldedCoros = &ScriptEngine::VMs.at(vmud->VM).YieldedCoroutines;

    for (ScriptEngine::YieldedCoroutine& yc : *yieldedCoros)
    {
        if (yc.Mode != ScriptEngine::YieldedCoroutine::ResumptionMode::DeferredEventResumption)
            continue;

        if (yc.RmEventCallback.Event == ec->Event && yc.RmEventCallback.Reflector == ec->Reflector && yc.RmEventCallback.ConnectionId == ec->ConnectionId)
            yc.Dead = true;
    }

    lua_unref(L, ec->EThreadRef);
    lua_unref(L, ec->CThreadRef);
    lua_unref(L, ec->SpawningThreadRef);
    decrementReflectorRefs(ec->Reflector);

    ec->ConnectionId = UINT32_MAX;
}

static int sig_namecall(lua_State* L)
{
    if (strcmp(lua_namecallatom(L, nullptr), "Connect") == 0)
    {
        luaL_checktype(L, 2, LUA_TFUNCTION);

        EventSignalData* ev = (EventSignalData*)luaL_checkudatatagged(L, 1, UserdataTag::EventSignal);
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
        int cLThreadRef = lua_ref(L, -1);
        int eLThreadRef = lua_ref(L, -1);
        lua_xpush(L, eL, 2); // push callback onto eL
        lua_pop(L, 1);

        EventConnectionData* ec = (EventConnectionData*)lua_newuserdatataggedwithmetatable(eL, sizeof(EventConnectionData), UserdataTag::EventConnection);
        *ec = {};
        lua_xpush(eL, L, -1);

        // assign the connection to the Event Thread so it can be used by the Connection Threads
        // `eL` itself is never resumed, only `cL` or `nL` (`nL` being created per-invocation if the thread yields every time)
        lua_rawsetptagged(eL, LUA_ENVIRONINDEX, eL, LightUserdataTag::EventConnectionData);

        lua_getglobal(L, "game");
        Reflection::GenericValue dmgv = ScriptEngine::L::ToGeneric(L, -1);
        if (dmgv.Type == Reflection::ValueType::GameObject)
        {
            GameObject* dm = GameObjectManager::Get()->FromGenericValue(dmgv);
            ec->DataModel = dm;
        }
        lua_pop(L, 1);

        ReflectorRef reflector = ev->Reflector;
        ObjectRef dmRef = ec->DataModel;
        dmRef->IncrementHardRefs();

        lua_xpush(eL, cL, 2);

        uint32_t cnId = rev->Connect(
            ev->Reflector.Referred(),
            Reflection::EventConnection{
                .Callback = [eL, ec, cL, rev, ev, dmRef, reflector](const std::vector<Reflection::GenericValue>& Inputs, uint32_t ConnectionId, uint32_t FromDataModel) -> void
                {
                    ZoneScopedN("QueueEvent");
                    queueEvent(eL, ec, cL, rev, ev, dmRef, reflector, Inputs, ConnectionId, FromDataModel);
                },
                .Cleanup = [=]()
                {
                    ZoneScopedN("CleanupEventConnection");
                    cleanupConnection(L, ec);
                    dmRef->DecrementHardRefs();
                },
                .DataModel = ev->RestrictDataModel,
            }
        );

        ec->Reflector = ev->Reflector;
        ec->SignalRef = signalRef;
        ec->EThreadRef = eLThreadRef;
        ec->CThreadRef = cLThreadRef;
        ec->ConnectionId = cnId;
        ec->Event = rev;
        ec->L = L;

        lua_pushthread(L);
        ec->SpawningThreadRef = lua_ref(L, -1);
        lua_pop(L, 1);

        ((ScriptEngine::L::StateUserdata*)lua_getthreaddata(L))->EventConnections.push_back(ec);

        incrementReflectorRefs(reflector);
        return 1;
    }
    else if (strcmp(lua_namecallatom(L, nullptr), "Wait") == 0)
    {
        EventSignalData* ev = (EventSignalData*)luaL_checkudatatagged(L, 1, UserdataTag::EventSignal);
        const Reflection::EventDescriptor* rev = ev->Event;

        ReflectorRef reflector = ev->Reflector;
        bool* resume = new bool;
        std::vector<Reflection::GenericValue>* values = new std::vector<Reflection::GenericValue>;
        uint32_t* fromDataModel = new uint32_t;
        *resume = false;

        EventConnectionData* ec = (EventConnectionData*)lua_newuserdatataggedwithmetatable(L, sizeof(EventConnectionData), UserdataTag::EventConnection);
        int cref = lua_ref(L, -1);
        *ec = {};

        ec->ConnectionId = rev->Connect(
            reflector.Referred(),
            Reflection::EventConnection{
                .Callback = [resume, rev, reflector, values, fromDataModel, ec](const std::vector<Reflection::GenericValue>& Values, uint32_t, uint32_t FromDataModel)
                -> void
                {
                    *values = Values;
                    *fromDataModel = FromDataModel;
                    *resume = true;
                },
                .Cleanup = [=]()
                {
                    cleanupConnection(L, ec);

                    if (!*resume)
                    {
                        std::string warning;
                        ScriptEngine::L::DumpStacktrace(L, &warning, 0, "Event was cleaned up, thread will not resume");

                        Log.Warning(warning);
                    }

                    delete resume;
                    delete values;
                    delete fromDataModel;
                }
            }
        );

        ec->Reflector = ev->Reflector;
        ec->SignalRef = cref;
        ec->ConnectionId = ec->ConnectionId;
        ec->Event = rev;
        ec->L = L;

        ((ScriptEngine::L::StateUserdata*)lua_getthreaddata(L))->EventConnections.push_back(ec);
        incrementReflectorRefs(reflector);

        return ScriptEngine::L::Yield(
            L,
            0,
            [=](ScriptEngine::YieldedCoroutine& yc)
            -> void
            {
                yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Polled;
                yc.RmPoll = [=](lua_State* L) -> int
                    {
                        if (*resume)
                        {
                            if (ev->RestrictDataModel != UINT32_MAX && *fromDataModel != UINT32_MAX && *fromDataModel != ev->RestrictDataModel)
                            {
                                GameObjectManager* objectManager = GameObjectManager::Get();
                                if (objectManager->FindById(ev->RestrictDataModel)->OwningDataModel != objectManager->FindById(*fromDataModel)->OwningDataModel)
                                {
                                    *resume = false;
                                    values->clear();
                                    return -1; // not our target dm
                                }
                            }

                            int count = (int)values->size();

                            for (const Reflection::GenericValue& gv : *values)
                                ScriptEngine::L::PushGenericValue(L, gv);

                            rev->Disconnect(reflector.Referred(), ec->ConnectionId);
                            return count;
                        }

                        return -1;
                    };
            }
        );
    }
    else
        luaL_error(L, "No such method of Event Signal known as '%s'", lua_namecallatom(L, nullptr));
}

static int sig_eq(lua_State* L)
{
    EventSignalData* ev1 = (EventSignalData*)luaL_checkudatatagged(L, 1, UserdataTag::EventSignal);
    EventSignalData* ev2 = (EventSignalData*)luaL_checkudatatagged(L, 2, UserdataTag::EventSignal);

    lua_pushboolean(L, ev1->Event == ev2->Event && ev1->Reflector == ev2->Reflector);
    return 1;
}

static int sig_tostring(lua_State* L)
{
    EventSignalData* ev = (EventSignalData*)luaL_checkudatatagged(L, 1, UserdataTag::EventSignal);

    if (ev->Reflector.Type == EntityComponent::None)
    {
        GameObject* target = (GameObject*)ev->Reflector.Referred();
        PHX_ENSURE(target);
        lua_pushfstring(L, "%s->%s", target->GetFullName().c_str(), ev->EventName);
    }
    else
    {
        lua_pushfstring(L, "%s::%s", s_EntityComponentNames[ev->Reflector.Type].data(), ev->EventName);
    }

    return 1;
}

static void createmetatable(lua_State* L)
{
    lua_createtable(L, 0, 4);

    lua_pushstring(L, "EventSignal");
    lua_setfield(L, -2, "__type");

    lua_pushcfunction(L, sig_namecall, "__namecall");
    lua_setfield(L, -2, "__namecall");

    lua_pushcfunction(L, sig_eq, "EventSignal.__eq");
    lua_setfield(L, -2, "__eq");

    lua_pushcfunction(L, sig_tostring, "EventSignal.__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_setuserdatametatable(L, UserdataTag::EventSignal);
}

int luhxopen_EventSignal(lua_State* L)
{
    createmetatable(L);
    return 0;
}
