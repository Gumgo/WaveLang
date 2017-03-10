#ifndef WAVELANG_COMMON_UTILITY_HASH_TABLE_H__
#define WAVELANG_COMMON_UTILITY_HASH_TABLE_H__

#include "common/common.h"
#include "common/utility/pod.h"

#include <new>
#include <vector>

// This hash table preallocates its memory
template<typename t_key, typename t_value, typename t_hash>
class c_hash_table {
public:
	c_hash_table();
	~c_hash_table();

	void initialize(size_t bucket_count, size_t element_count);
	void terminate();

	// Returns pointer to the value if successful, null pointer otherwise
	t_value *insert(const t_key &key); // Uses default constructor for value
	t_value *insert(const t_key &key, const t_value &value);

	bool remove(const t_key &key);

	t_value *find(const t_key &key);
	const t_value *find(const t_key &key) const;

private:
	static const size_t k_invalid_element_index = static_cast<size_t>(-1);

	typedef s_pod<t_key> s_pod_key;
	typedef s_pod<t_value> s_pod_value;

	struct s_bucket {
		size_t first_element_index;
	};

	struct s_element {
		size_t next_element_index;
		s_pod_key key;
		s_pod_value value;
	};

	size_t get_bucket_index(const t_key &key) const;
	s_element *insert_internal(const t_key &key);

	std::vector<s_bucket> m_buckets;
	std::vector<s_element> m_elements;
	size_t m_free_list;
};

#include "common/utility/hash_table.inl"

#endif // WAVELANG_COMMON_UTILITY_HASH_TABLE_H__