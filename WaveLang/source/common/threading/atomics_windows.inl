inline void c_atomic_int32::initialize(int32 value) {
	m_value = value;
}

inline int32 c_atomic_int32::get_unsafe() const {
	return m_value;
}

inline void c_atomic_int64::initialize(int64 value) {
	m_value = value;
}

inline int64 c_atomic_int64::get_unsafe() const {
	return m_value;
}

template<typename t_value> struct s_atomic_operation_and {
	t_value x;
	s_atomic_operation_and(t_value x) : x(x) {}
	t_value operator()(t_value v) const { return x & v; }
};

template<typename t_value> struct s_atomic_operation_or {
	t_value x;
	s_atomic_operation_or(t_value x) : x(x) {}
	t_value operator()(t_value v) const { return x | v; }
};

template<typename t_value> struct s_atomic_operation_xor {
	t_value x;
	s_atomic_operation_xor(t_value x) : x(x) {}
	t_value operator()(t_value v) const { return x ^ v; }
};

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

static_assert(sizeof(volatile unsigned long) == sizeof(volatile int32), "Atomic size mismatch");
static_assert(sizeof(volatile unsigned long long) == sizeof(volatile int64), "Atomic size mismatch");

static volatile unsigned long *to_dst32(volatile int32 *x) {
	return reinterpret_cast<volatile unsigned long *>(x);
}

static unsigned long to_src32(int32 x) {
	return static_cast<unsigned long>(x);
}

static volatile unsigned long long *to_dst64(volatile int64 *x) {
	return reinterpret_cast<volatile unsigned long long *>(x);
}

static unsigned long long to_src64(int64 x) {
	return static_cast<unsigned long long>(x);
}

inline int32 c_atomic_int32::get() {
	// Compare/exchange with some arbitrary value
	return InterlockedCompareExchange(to_dst32(&m_value), 0xabcdabcd, 0xabcdabcd);
}

inline int32 c_atomic_int32::exchange(int32 x) {
	return InterlockedExchange(to_dst32(&m_value), to_src32(x));
}

inline int32 c_atomic_int32::compare_exchange(int32 c, int32 x) {
	return InterlockedCompareExchange(to_dst32(&m_value), to_src32(x), to_src32(c));
}

inline int32 c_atomic_int32::add(int32 x) {
	return InterlockedExchangeAdd(to_dst32(&m_value), to_src32(x)) - x;
}

inline int32 c_atomic_int32::increment() {
	return InterlockedIncrement(to_dst32(&m_value)) - 1;
}

inline int32 c_atomic_int32::decrement() {
	return InterlockedDecrement(to_dst32(&m_value)) + 1;
}

// Seems to not be 32-bit versions of these functions defined except for IA64

inline int32 c_atomic_int32::and(int32 x) {
	return execute_atomic(s_atomic_operation_and<int32>(x));
}

inline int32 c_atomic_int32::or(int32 x) {
	return execute_atomic(s_atomic_operation_or<int32>(x));
}

inline int32 c_atomic_int32::xor(int32 x) {
	return execute_atomic(s_atomic_operation_xor<int32>(x));
}

inline int64 c_atomic_int64::get() {
	// Compare/exchange with some arbitrary value
	return InterlockedCompareExchange(to_dst64(&m_value), 0xabcdabcdabcdabcdl, 0xabcdabcdabcdabcdl);
}

inline int64 c_atomic_int64::exchange(int64 x) {
	return InterlockedExchange(to_dst64(&m_value), to_src64(x));
}

inline int64 c_atomic_int64::compare_exchange(int64 c, int64 x) {
	return InterlockedCompareExchange(to_dst64(&m_value), to_src64(x), to_src64(c));
}

inline int64 c_atomic_int64::add(int64 x) {
	return InterlockedExchangeAdd(to_dst64(&m_value), to_src64(x)) - x;
}

inline int64 c_atomic_int64::increment() {
	return InterlockedIncrement(to_dst64(&m_value)) - 1;
}

inline int64 c_atomic_int64::decrement() {
	return InterlockedDecrement(to_dst64(&m_value)) + 1;
}

inline int64 c_atomic_int64::and(int64 x) {
	return InterlockedAnd(to_dst64(&m_value), to_src64(x));
}

inline int64 c_atomic_int64::or(int64 x) {
	return InterlockedOr(to_dst64(&m_value), to_src64(x));
}

inline int64 c_atomic_int64::xor(int64 x) {
	return InterlockedXor(to_dst64(&m_value), to_src64(x));
}