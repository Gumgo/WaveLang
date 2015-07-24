// Non platform-specific functions first:

static_assert(sizeof(std::atomic<int32>) == sizeof(int32), "std::atomic<int32> not 32 bits?");
static_assert(sizeof(std::atomic<int64>) == sizeof(int64), "std::atomic<int64> not 64 bits?");

inline void c_atomic_int32::initialize(int32 value) {
	// This is probably undefined behavior, but I'm pretty sure it's fine...
	*reinterpret_cast<int32 *>(&m_value) = value;
}

inline int32 c_atomic_int32::get_unsafe() const {
	return *reinterpret_cast<const int32 *>(&m_value);
}

inline void c_atomic_int64::initialize(int64 value) {
	// This is probably undefined behavior, but I'm pretty sure it's fine...
	*reinterpret_cast<int64 *>(&m_value) = value;
}

inline int64 c_atomic_int64::get_unsafe() const {
	return *reinterpret_cast<const int64 *>(&m_value);
}

// Performs atomic operations which aren't directly supported. It does this by computing the desired value
// non-atomically, and calling compareExchange until the returned value equals the original, previous value.
template<typename t_operation>
int32 c_atomic_int32::execute_atomic(const t_operation &operation) {
	int32 original, result;
	do {
		original = m_value;
		result = operation(original);
	} while (compare_exchange(original, result) != original);
	return original;
}

// Performs atomic operations which aren't directly supported. It does this by computing the desired value
// non-atomically, and calling compareExchange until the returned value equals the original, previous value.
template<typename t_operation>
int64 c_atomic_int64::execute_atomic(const t_operation &operation) {
	int64 original, result;
	do {
		original = m_value;
		result = operation(original);
	} while (compare_exchange(original, result) != original);
	return original;
}

inline int32 c_atomic_int32::get() {
	// Compare/exchange with some arbitrary value
	return m_value.load();
}

inline int32 c_atomic_int32::exchange(int32 x) {
	return m_value.exchange(x);
}

inline int32 c_atomic_int32::compare_exchange(int32 c, int32 x) {
	int32 expected = c;
	// If the exchange fails, expected is replaced with the current value
	m_value.compare_exchange_strong(expected, x);
	return expected;
}

inline int32 c_atomic_int32::add(int32 x) {
	m_value.fetch_add(x);
}

inline int32 c_atomic_int32::increment() {
	return m_value++;
}

inline int32 c_atomic_int32::decrement() {
	return m_value--;
}

// Seems to not be 32-bit versions of these functions defined except for IA64

inline int32 c_atomic_int32::and(int32 x) {
	return m_value.fetch_and(x);
}

inline int32 c_atomic_int32::or(int32 x) {
	return m_value.fetch_or(x);
}

inline int32 c_atomic_int32::xor(int32 x) {
	return m_value.fetch_xor(x);
}

inline int64 c_atomic_int64::get() {
	return m_value.load();
}

inline int64 c_atomic_int64::exchange(int64 x) {
	return m_value.exchange(x);
}

inline int64 c_atomic_int64::compare_exchange(int64 c, int64 x) {
	int64 expected = c;
	// If the exchange fails, expected is replaced with the current value
	m_value.compare_exchange_strong(expected, x);
	return expected;
}

inline int64 c_atomic_int64::add(int64 x) {
	return m_value.fetch_add(x);
}

inline int64 c_atomic_int64::increment() {
	return m_value++;
}

inline int64 c_atomic_int64::decrement() {
	return m_value--;
}

inline int64 c_atomic_int64::and(int64 x) {
	return m_value.fetch_and(x);
}

inline int64 c_atomic_int64::or(int64 x) {
	return m_value.fetch_or(x);
}

inline int64 c_atomic_int64::xor(int64 x) {
	return m_value.fetch_xor(x);
}
