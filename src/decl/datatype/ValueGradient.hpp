// A sequence of keypoints that can be smoothly interpolated between

#pragma once

#include <vector>
#include <random>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

template <class T>
class ValueGradientKeypoint
{
public:
	ValueGradientKeypoint(float Time, T Value, float Envelope = 0.f);

	float Time; // Time in the sequence
	T Value; // Value at this point in time
	float Envelope; // Random deviation
};

template <class T>
class ValueGradient
{ //IMPORTANT: ValueGradient<type>: 'type' should support math operations - '-', '*', '+'
public:
	ValueGradient(const std::vector<ValueGradientKeypoint<T>>& InitKeys);
	ValueGradient() = default;

	T GetValue(float Time);
	void InsertKey(const ValueGradientKeypoint<T>& Key);
	void InsertKey(float Time, T Value, float Envelope = 0.f);

	std::vector<ValueGradientKeypoint<T>> GetKeys();
	std::vector<T> GetKeysValues();

private:
	std::vector<ValueGradientKeypoint<T>> m_Keys;
};

template <class T>
ValueGradientKeypoint<T>::ValueGradientKeypoint(float time, T value, float envelope)
	: Time(time), Value(value), Envelope(envelope)
{
}

template <class T>
ValueGradient<T>::ValueGradient(const std::vector<ValueGradientKeypoint<T>>& initialKeys)
	: m_Keys(initialKeys)
{
}

template <typename T>
bool m_Compare(ValueGradientKeypoint<T> A, ValueGradientKeypoint<T> B)
{
	return (A.Time < B.Time);
}

template <class T>
T ValueGradient<T>::GetValue(float Time)
{
	if (m_Keys.empty())
		return T();

	if (m_Keys.size() == 1)
		return m_Keys[0].Value;

	static std::default_random_engine RandGenerator;
	static std::uniform_real_distribution<float> RealDistribution{ -1.f, 1.f };

	float deviation = RealDistribution(RandGenerator);

	if (Time == 0.f)
		return m_Keys[0].Value + (m_Keys[0].Envelope * deviation);

	if (Time == 1.f)
		return m_Keys.back().Value + (m_Keys[m_Keys.size() - 1].Envelope * deviation);

	for (size_t keyIndex = 0; keyIndex < (m_Keys.size() - 1); keyIndex++)
	{
		ValueGradientKeypoint<T> currentKey = m_Keys[keyIndex];
		ValueGradientKeypoint<T> nextKey = m_Keys[keyIndex + 1]; //Will always exist because of Keys.size() - 1

		if (Time >= currentKey.Time && Time < nextKey.Time)
		{
			float alpha = (Time - currentKey.Time) / (nextKey.Time - currentKey.Time);

			float enveLerp = currentKey.Envelope + (nextKey.Envelope - currentKey.Envelope) * alpha;

			return ((nextKey.Value - currentKey.Value) * alpha + currentKey.Value) + (enveLerp * deviation);
		}
	}

	return T();
}

template <class T>
std::vector<ValueGradientKeypoint<T>> ValueGradient<T>::GetKeys()
{
	return m_Keys;
}

template <class T>
std::vector<T> ValueGradient<T>::GetKeysValues()
{
	std::vector<T> values;
	values.reserve(m_Keys.size());

	for (const ValueGradientKeypoint<T> key : m_Keys)
		values.push_back(key.Value);

	return values;
}

template <class T>
void ValueGradient<T>::InsertKey(const ValueGradientKeypoint<T>& Key)
{
	m_Keys.push_back(Key);

	//std::sort(m_Keys.begin(), m_Keys.end(), m_compare);
}

template <class T>
void ValueGradient<T>::InsertKey(float Time, T Value, float Envelope)
{
	m_Keys.emplace_back(Time, Value, Envelope);
}

#ifndef VALUEGRADIENT_NO_EXTERN_COMMON

extern template class ValueGradientKeypoint<float>;
extern template class ValueGradientKeypoint<double>;
extern template class ValueGradientKeypoint<glm::vec2>;
extern template class ValueGradientKeypoint<glm::vec3>;

extern template class ValueGradient<float>;
extern template class ValueGradient<double>;
extern template class ValueGradient<glm::vec2>;
extern template class ValueGradient<glm::vec3>;

#endif
