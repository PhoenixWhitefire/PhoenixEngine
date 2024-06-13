//Template implementation file
//To fix "unresolved external symbol" compile error with class templates

template <class _vtype> ValueRangeKey<_vtype>::ValueRangeKey(_vtype value, float time) {
	this->Value = value;
	this->Time = time;
}

template <class ValueType> ValueRange<ValueType>::ValueRange(std::vector<ValueRangeKey<ValueType>> InitKeys) {
	this->m_keys = InitKeys;
}

template <class ValueType> ValueRange<ValueType>::ValueRange() {

}

template <typename T> bool m_compare(ValueRangeKey<T> A, ValueRangeKey<T> B) {
	return (A.Time < B.Time);
}

template <class ValueType> ValueType ValueRange<ValueType>::GetValue(float Time) {
	if (this->m_keys.size() < 2) {
		return ValueType();
	}

	if (Time == 0) {
		return this->m_keys[0].Value;
	}

	if (Time == 1) {
		return this->m_keys[this->m_keys.size() - 1].Value;
	}

	for (int KeyIndex = 0; KeyIndex < (this->m_keys.size() - 1); KeyIndex++) {
		ValueRangeKey<ValueType> CurrentKey = this->m_keys[KeyIndex];
		ValueRangeKey<ValueType> NextKey = this->m_keys[KeyIndex + 1]; //Will always exist because of Keys.size() - 1

		if (Time >= CurrentKey.Time && Time < NextKey.Time) {
			float Alpha = (Time - CurrentKey.Time) / (NextKey.Time - CurrentKey.Time);

			return (NextKey.Value - CurrentKey.Value) * Alpha + CurrentKey.Value;
		}
	}

	return ValueType();
}

template <class ValueType> std::vector<ValueRangeKey<ValueType>> ValueRange<ValueType>::GetKeys() {
	return this->m_keys;
}

template <class ValueType> std::vector<ValueType> ValueRange<ValueType>::GetKeysValues() {
	std::vector<ValueType> values;

	for (int Index = 0; Index < this->m_keys.size(); Index++) {
		values.push_back(this->m_keys[Index].Value);
	}

	return values;
}

template <class ValueType> void ValueRange<ValueType>::InsertKey(ValueRangeKey<ValueType> Key) {
	this->m_keys.push_back(Key);

	//std::sort(this->m_keys.begin(), this->m_keys.end(), m_compare);
}
