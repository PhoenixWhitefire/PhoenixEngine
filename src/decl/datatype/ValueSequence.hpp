// Contains both the declarations and the
// implementation due to wackyness with templates

#pragma once

#include <vector>
#include <random>

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
	ValueSequence(const std::vector<ValueSequenceKeypoint<T>>& InitKeys);
	ValueSequence();

	T GetValue(float Time);
	void InsertKey(const ValueSequenceKeypoint<T>& Key);

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

template <class T> ValueSequence<T>::ValueSequence(const std::vector<ValueSequenceKeypoint<T>>& initialKeys)
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
	if (m_Keys.empty())
		return T();

	if (m_Keys.size() == 1)
		return m_Keys[0].Value;

	//float deviation = EnvelopDist(RandGenerator);

	if (Time == 0.f)
		return m_Keys[0].Value;// + (m_Keys[0].Envelope * deviation);

	if (Time == 1.f)
		return m_Keys[m_Keys.size() - 1].Value;// + (m_Keys[m_Keys.size() - 1].Envelope * deviation);

	for (size_t keyIndex = 0; keyIndex < (m_Keys.size() - 1); keyIndex++)
	{
		ValueSequenceKeypoint<T> currentKey = m_Keys[keyIndex];
		ValueSequenceKeypoint<T> nextKey = m_Keys[keyIndex + 1]; //Will always exist because of Keys.size() - 1

		if (Time >= currentKey.Time && Time < nextKey.Time)
		{
			float alpha = (Time - currentKey.Time) / (nextKey.Time - currentKey.Time);

			//float enveLerp = currentKey.Envelope + (nextKey.Envelope - currentKey.Envelope) * alpha;

			return ((nextKey.Value - currentKey.Value) * alpha + currentKey.Value);// + (enveLerp * deviation);
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
	values.reserve(m_Keys.size());

	for (const ValueSequenceKeypoint<T> key : m_Keys)
		values.push_back(key.Value);

	return values;
}

template <class T> void ValueSequence<T>::InsertKey(const ValueSequenceKeypoint<T>& Key)
{
	m_Keys.push_back(Key);

	//std::sort(m_Keys.begin(), m_Keys.end(), m_compare);
}
