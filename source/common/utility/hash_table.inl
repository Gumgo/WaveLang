template<typename t_key, typename t_value, typename t_hash>
c_hash_table<t_key, t_value, t_hash>::c_hash_table() {
	m_free_list = k_invalid_element_index;
}

template<typename t_key, typename t_value, typename t_hash>
c_hash_table<t_key, t_value, t_hash>::~c_hash_table() {
	terminate();
}

template<typename t_key, typename t_value, typename t_hash>
void c_hash_table<t_key, t_value, t_hash>::initialize(size_t bucket_count, size_t element_count) {
	terminate();

	wl_assert(m_buckets.empty());
	wl_assert(m_elements.empty());
	wl_assert(m_free_list = k_invalid_element_index);
	wl_assert(bucket_count > 0);
	wl_assert(element_count > 0);

	m_buckets.resize(bucket_count);
	for (size_t bucket_index = 0; bucket_index < bucket_count; bucket_index++) {
		m_buckets[bucket_index].first_element_index = k_invalid_element_index;
	}

	m_elements.resize(element_count);

	m_free_list = 0;
	for (size_t element_index = 0; element_index < element_count; element_index++) {
		m_elements[element_index].next_element_index = (element_index + 1 == element_count) ?
			k_invalid_element_index :
			element_index + 1;
	}
}

template<typename t_key, typename t_value, typename t_hash>
void c_hash_table<t_key, t_value, t_hash>::terminate() {
	// Explicitly call destructor on all elements
	for (size_t bucket_index = 0; bucket_index < m_buckets.size(); bucket_index++) {
		size_t element_index = m_buckets[bucket_index].first_element_index;
		while (element_index != k_invalid_element_index) {
			s_element &element = m_elements[element_index];
			element.key.get().~t_key();
			element.value.get().~t_value();
			element_index = element.next_element_index;
		}
	}

	m_buckets.clear();
	m_elements.clear();
	m_free_list = k_invalid_element_index;
}

template<typename t_key, typename t_value, typename t_hash>
t_value *c_hash_table<t_key, t_value, t_hash>::insert(const t_key &key) {
	s_element *new_element = insert_internal(key);

	if (new_element) {
		new(new_element->key.get_pointer()) t_key(key);
		new(new_element->value.get_pointer()) t_value();
		return new_element->value.get_pointer();
	} else {
		return nullptr;
	}
}

template<typename t_key, typename t_value, typename t_hash>
t_value *c_hash_table<t_key, t_value, t_hash>::insert(const t_key &key, const t_value &value) {
	s_element *new_element = insert_internal(key);

	if (new_element) {
		new(new_element->key.get_pointer()) t_key(key);
		new(new_element->value.get_pointer()) t_value(value);
		return new_element->value.get_pointer();
	} else {
		return nullptr;
	}
}

template<typename t_key, typename t_value, typename t_hash>
bool c_hash_table<t_key, t_value, t_hash>::remove(const t_key &key) {
	size_t bucket_index = get_bucket_index(key);
	s_bucket &bucket = m_buckets[bucket_index];

	size_t prev_element_index = k_invalid_element_index;
	size_t element_index = bucket.first_element_index;

	while (element_index != k_invalid_element_index) {
		s_element &element = m_elements[element_index];

		if (key == element.key.get()) {
			// Found a match
			element.key.get().~t_key();
			element.value.get().~t_value();
			if (prev_element_index == k_invalid_element_index) {
				bucket.first_element_index = element.next_element_index;
			} else {
				m_elements[prev_element_index].next_element_index = element.next_element_index;
			}

			element.next_element_index = m_free_list;
			m_free_list = element_index;

			return true;
		}

		prev_element_index = element_index;
		element_index = element.next_element_index;
	}

	return false;
}

template<typename t_key, typename t_value, typename t_hash>
t_value *c_hash_table<t_key, t_value, t_hash>::find(const t_key &key) {
	size_t bucket_index = get_bucket_index(key);
	const s_bucket &bucket = m_buckets[bucket_index];

	size_t prev_element_index = k_invalid_element_index;
	size_t element_index = bucket.first_element_index;

	while (element_index != k_invalid_element_index) {
		s_element &element = m_elements[element_index];

		if (key == element.key.get()) {
			// Found a match
			return element.value.get_pointer();
		}

		prev_element_index = element_index;
		element_index = element.next_element_index;
	}

	return nullptr;
}

template<typename t_key, typename t_value, typename t_hash>
const t_value *c_hash_table<t_key, t_value, t_hash>::find(const t_key &key) const {
	size_t bucket_index = get_bucket_index(key);
	const s_bucket &bucket = m_buckets[bucket_index];

	size_t prev_element_index = k_invalid_element_index;
	size_t element_index = bucket.first_element_index;

	while (element_index != k_invalid_element_index) {
		const s_element &element = m_elements[element_index];

		if (key == element.key.get()) {
			// Found a match
			return element.value.get_pointer();
		}

		prev_element_index = element_index;
		element_index = element.next_element_index;
	}

	return nullptr;
}

template<typename t_key, typename t_value, typename t_hash>
size_t c_hash_table<t_key, t_value, t_hash>::get_bucket_index(const t_key &key) const {
	return t_hash()(key) % m_buckets.size();
}

template<typename t_key, typename t_value, typename t_hash>
typename c_hash_table<t_key, t_value, t_hash>::s_element *c_hash_table<t_key, t_value, t_hash>::insert_internal(
	const t_key &key) {
	if (m_free_list == k_invalid_element_index) {
		return nullptr;
	}

	size_t bucket_index = get_bucket_index(key);
	s_bucket &bucket = m_buckets[bucket_index];

	size_t element_index = m_free_list;
	s_element &element = m_elements[element_index];
	m_free_list = element.next_element_index;
	element.next_element_index = bucket.first_element_index;
	bucket.first_element_index = element_index;

	return &element;
}
