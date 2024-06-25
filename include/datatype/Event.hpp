// Contains both the declarations and the
// implementation due to wackyness with templates

#pragma once

#include<functional>
#include<vector>

template <class _ptype> class EventConnection
{
public:
	EventConnection(std::function<void(_ptype)> Function);

	void Disconnect();
	void Fire(_ptype Value);

	bool Connected = true;
private:
	std::function<void(_ptype)> m_fnConnectedFunction;
};

template <class ParamType> class Event
{
public:
	EventConnection<ParamType> Connect(std::function<void(ParamType)> Function);
	void DisconnectAll();
	void Fire(ParamType Value);

	std::vector<EventConnection<ParamType>> GetConnections();

private:
	std::vector<EventConnection<ParamType>> m_vcConnections;
};

template <class _ptype> EventConnection<_ptype>::EventConnection(std::function<void(_ptype)> Function)
{
	this->m_fnConnectedFunction = Function;
}

template <class _ptype> void EventConnection<_ptype>::Disconnect()
{
	this->Connected = false;
}

template <class _ptype> void EventConnection<_ptype>::Fire(_ptype Value)
{
	this->m_fnConnectedFunction(Value);
}

template <class ParamType> EventConnection<ParamType> Event<ParamType>::Connect(std::function<void(ParamType)> Function)
{
	EventConnection<ParamType> NewConnection = EventConnection<ParamType>(Function);

	this->m_vcConnections.push_back(NewConnection);

	return NewConnection;
}

template <class ParamType> void Event<ParamType>::DisconnectAll()
{
	for (int Index = 0; Index < this->m_vcConnections.size(); Index++) {
		this->m_vcConnections[Index].Disconnect();
	}
}

template <class ParamType> void Event<ParamType>::Fire(ParamType Value)
{
	for (int Index = 0; Index < this->m_vcConnections.size(); Index++) {
		((EventConnection<ParamType>)this->m_vcConnections[Index]).Fire(Value);
	}
}

template <class ParamType> std::vector<EventConnection<ParamType>> Event<ParamType>::GetConnections()
{
	return this->m_vcConnections;
}
