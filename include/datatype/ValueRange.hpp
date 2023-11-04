#pragma once

#include<algorithm>
#include<vector>

template <class _vtype> class ValueRangeKey {
	public:
		ValueRangeKey(_vtype value, float time);

		_vtype Value;
		float Time;
};

template <class ValueType> class ValueRange { //IMPORTANT: ValueRange<type>: 'type' should support math operations - '-', '*', '+'
	public:
		ValueRange(std::vector<ValueRangeKey<ValueType>> InitKeys);
		ValueRange();

		ValueType GetValue(float Time);
		void InsertKey(ValueRangeKey<ValueType> Key);

		std::vector<ValueRangeKey<ValueType>> GetKeys();
		std::vector<ValueType> GetKeysValues();
	private:
		std::vector<ValueRangeKey<ValueType>> m_keys;
};

#include"ValueRange.ipp"
