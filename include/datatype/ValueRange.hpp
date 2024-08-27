// Contains both the declarations and the
// implementation due to wackyness with templates

#pragma once

#include<vector>
#include<random>

template <class T> class ValueSequenceKeypoint
{
public:
	ValueSequenceKeypoint(float time, T value);

	float Time; // Time in the sequence
	T Value; // Value at this point in time
	float Envelope; // Random deviation
};

template <class T> class ValueSequence
{ //IMPORTANT: ValueSequence<type>: 'type' should support math operations - '-', '*', '+'
public:
	ValueSequence(std::vector<ValueSequenceKeypoint<T>> InitKeys);
	ValueSequence();

	T GetValue(float Time);
	void InsertKey(ValueSequenceKeypoint<T> Key);

	std::vector<ValueSequenceKeypoint<T>> GetKeys();
	std::vector<T> GetKeysValues();

private:
	std::vector<ValueSequenceKeypoint<T>> m_Keys;
};

static std::default_random_engine RandGenerator = std::default_random_engine(static_cast<unsigned int>(time(NULL)));
static std::uniform_real_distribution<float> EnvelopDist(0.f, 1.f);

template <class T> ValueSequenceKeypoint<T>::ValueSequenceKeypoint(float time, T value)
	: Value(value), Time(time), Envelope(0.f)
{
}

template <class T> ValueSequence<T>::ValueSequence(std::vector<ValueSequenceKeypoint<T>> initialKeys)
	: m_Keys(initialKeys)
{
}

template <class T> ValueSequence<T>::ValueSequence()
{
}

template <typename T> bool m_compare(ValueSequenceKeypoint<T> A, ValueSequenceKeypoint<T> B)
{
	return (A.Time < B.Time);
}

template <class T> T ValueSequence<T>::GetValue(float Time)
{
	if (m_Keys.size() < 2)
		return T();

	float Deviation = EnvelopDist(RandGenerator);

	if (Time == 0)
		return m_Keys[0].Value * (m_Keys[0].Envelope * Deviation);

	if (Time == 1)
		return m_Keys[m_Keys.size() - 1].Value * (m_Keys[m_Keys.size() - 1].Envelope * Deviation);

	for (int KeyIndex = 0; KeyIndex < (m_Keys.size() - 1); KeyIndex++)
	{
		ValueSequenceKeypoint<T> CurrentKey = m_Keys[KeyIndex];
		ValueSequenceKeypoint<T> NextKey = m_Keys[KeyIndex + 1]; //Will always exist because of Keys.size() - 1

		if (Time >= CurrentKey.Time && Time < NextKey.Time)
		{
			float Alpha = (Time - CurrentKey.Time) / (NextKey.Time - CurrentKey.Time);

			float EnveLerp = CurrentKey.Envelope + (NextKey.Envelope - CurrentKey.Envelope) * Alpha;

			return ((NextKey.Value - CurrentKey.Value) * Alpha + CurrentKey.Value) * (EnveLerp * Deviation);
		}
	}

	return T();
}

template <class T> std::vector<ValueSequenceKeypoint<T>> ValueSequence<T>::GetKeys()
{
	return m_Keys;
}

template <class T> std::vector<T> ValueSequence<T>::GetKeysValues()
{
	std::vector<T> values;

	for (int Index = 0; Index < m_Keys.size(); Index++)
		values.push_back(m_Keys[Index].Value);

	return values;
}

template <class T> void ValueSequence<T>::InsertKey(ValueSequenceKeypoint<T> Key)
{
	m_Keys.push_back(Key);

	//std::sort(m_Keys.begin(), m_Keys.end(), m_compare);
}
