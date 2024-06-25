#include<random>

#include"datatype/ValueRange.hpp"

static std::default_random_engine RandGenerator = std::default_random_engine(time(NULL));
static std::uniform_real_distribution<double> EnvelopDist(0.f, 1.f);

template <class _vtype> ValueRangeKey<_vtype>::ValueRangeKey(float time, _vtype value)
{
	this->Value = value;
	this->Time = time;
}

template <class ValueType> ValueRange<ValueType>::ValueRange(std::vector<ValueRangeKey<ValueType>> InitKeys)
{
	this->m_keys = InitKeys;
}

template <class ValueType> ValueRange<ValueType>::ValueRange()
{

}

template <typename T> bool m_compare(ValueRangeKey<T> A, ValueRangeKey<T> B)
{
	return (A.Time < B.Time);
}

template <class ValueType> ValueType ValueRange<ValueType>::GetValue(float Time)
{
	if (this->m_keys.size() < 2)
		return ValueType();

	double Deviation = EnvelopDist(RandGenerator);

	if (Time == 0)
		return this->m_keys[0].Value * (this->m_keys[0].Envelope * Deviation);

	if (Time == 1)
		return this->m_keys[this->m_keys.size() - 1].Value * (this->m_keys[this->m_keys.size() - 1].Envelope * Deviation);

	for (int KeyIndex = 0; KeyIndex < (this->m_keys.size() - 1); KeyIndex++)
	{
		ValueRangeKey<ValueType> CurrentKey = this->m_keys[KeyIndex];
		ValueRangeKey<ValueType> NextKey = this->m_keys[KeyIndex + 1]; //Will always exist because of Keys.size() - 1

		if (Time >= CurrentKey.Time && Time < NextKey.Time)
		{
			float Alpha = (Time - CurrentKey.Time) / (NextKey.Time - CurrentKey.Time);

			float EnveLerp = CurrentKey.Envelope + (NextKey.Envelope - CurrentKey.Envelope) * Alpha;

			return ((NextKey.Value - CurrentKey.Value) * Alpha + CurrentKey.Value) * (EnveLerp * Deviation);
		}
	}

	return ValueType();
}

template <class ValueType> std::vector<ValueRangeKey<ValueType>> ValueRange<ValueType>::GetKeys()
{
	return this->m_keys;
}

template <class ValueType> std::vector<ValueType> ValueRange<ValueType>::GetKeysValues()
{
	std::vector<ValueType> values;

	for (int Index = 0; Index < this->m_keys.size(); Index++)
		values.push_back(this->m_keys[Index].Value);

	return values;
}

template <class ValueType> void ValueRange<ValueType>::InsertKey(ValueRangeKey<ValueType> Key)
{
	this->m_keys.push_back(Key);

	//std::sort(this->m_keys.begin(), this->m_keys.end(), m_compare);
}
