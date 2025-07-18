// Immediate event system

#pragma once

#include <functional>
#include <vector>

template <class T>
class EventConnection
{
public:
	EventConnection(std::function<void(T)> Function);

	void Disconnect();
	void Fire(T Value);

	bool Connected = true;

private:
	std::function<void(T)> m_Callback;
};

template <class T>
class EventSignal
{
public:
	size_t Connect(std::function<void(T)> Function);
	void DisconnectAll();
	void Fire(T Value);

	std::vector<EventConnection<T>>& GetConnections();

private:
	std::vector<EventConnection<T>> m_Connections;
};

template <class T>
EventConnection<T>::EventConnection(std::function<void(T)> function)
	: m_Callback(function)
{
}

template <class T>
void EventConnection<T>::Disconnect()
{
	this->Connected = false;
}

template <class T>
void EventConnection<T>::Fire(T Value)
{
	if (Connected)
		m_Callback(Value);
}

template <class T>
size_t EventSignal<T>::Connect(std::function<void(T)> function)
{
	EventConnection<T> newConnection{ function };
	m_Connections.push_back(newConnection);

	return m_Connections.size() - 1;
}

template <class T>
void EventSignal<T>::DisconnectAll()
{
	for (size_t Index = 0; Index < m_Connections.size(); Index++)
		m_Connections[Index].Disconnect();

	m_Connections.clear();
}

template <class T>
void EventSignal<T>::Fire(T Value)
{
	for (size_t Index = 0; Index < m_Connections.size(); Index++)
		m_Connections[Index].Fire(Value);
}

template <class T>
std::vector<EventConnection<T>>& EventSignal<T>::GetConnections()
{
	return m_Connections;
}
