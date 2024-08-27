// Contains both the declarations and the
// implementation due to wackyness with templates

#pragma once

#include<functional>
#include<vector>

template <class T> class EventConnection
{
public:
	EventConnection(std::function<void(T)> Function);

	void Disconnect();
	void Fire(T Value);

	bool Connected = true;

private:
	std::function<void(T)> m_Callback;
};

template <class T> class EventSignal
{
public:
	EventConnection<T> Connect(std::function<void(T)> Function);
	void DisconnectAll();
	void Fire(T Value);

	std::vector<EventConnection<T>> GetConnections();

private:
	std::vector<EventConnection<T>> m_Connections;
};

template <class T> EventConnection<T>::EventConnection(std::function<void(T)> function)
	: m_Callback(function)
{
}

template <class T> void EventConnection<T>::Disconnect()
{
	this->Connected = false;
}

template <class T> void EventConnection<T>::Fire(T Value)
{
	m_Callback(Value);
}

template <class T> EventConnection<T> EventSignal<T>::Connect(std::function<void(T)> function)
{
	EventConnection<T> newConnection = EventConnection<T>(function);
	m_Connections.push_back(newConnection);

	return newConnection;
}

template <class T> void EventSignal<T>::DisconnectAll()
{
	for (int Index = 0; Index < m_Connections.size(); Index++)
		m_Connections[Index].Disconnect();
}

template <class T> void EventSignal<T>::Fire(T Value)
{
	for (int Index = 0; Index < m_Connections.size(); Index++)
		((EventConnection<T>)m_Connections[Index]).Fire(Value);
}

template <class T> std::vector<EventConnection<T>> EventSignal<T>::GetConnections()
{
	return m_Connections;
}
