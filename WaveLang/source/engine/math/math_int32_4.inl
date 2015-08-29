inline c_int32_4::c_int32_4() {
}

inline c_int32_4::c_int32_4(int32 v)
	: m_value(_mm_set1_epi32(v)) {
}

inline c_int32_4::c_int32_4(int32 a, int32 b, int32 c, int32 d)
	: m_value(_mm_set_epi32(a, b, c, d)) {
}

inline c_int32_4::c_int32_4(const int32 *ptr) {
	wl_assert(is_pointer_aligned(ptr, k_sse_alignment));
	m_value = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr));
}

inline c_int32_4::c_int32_4(const __m128i &v)
	: m_value(v) {
}

inline c_int32_4::c_int32_4(const c_int32_4 &v)
	: m_value(v) {
}

inline void c_int32_4::load(const int32 *ptr) {
	wl_assert(is_pointer_aligned(ptr, k_sse_alignment));
	m_value = _mm_load_si128(reinterpret_cast<const __m128i *>(ptr));
}

inline void c_int32_4::store(int32 *ptr) const {
	wl_assert(is_pointer_aligned(ptr, k_sse_alignment));
	_mm_store_si128(reinterpret_cast<__m128i *>(ptr), m_value);
}

inline c_int32_4 &c_int32_4::operator=(const __m128i &v) {
	m_value = v;
	return *this;
}

inline c_int32_4 &c_int32_4::operator=(const c_int32_4 &v) {
	m_value = v;
	return *this;
}

inline c_int32_4::operator __m128i() const {
	return m_value;
}

inline c_real32_4 c_int32_4::real32_4_from_bits() const {
	return _mm_castsi128_ps(m_value);
}

inline c_int32_4 operator+(const c_int32_4 &v) {
	return v;
}

inline c_int32_4 operator-(const c_int32_4 &v) {
	return _mm_sub_epi32(_mm_set1_epi32(0), v);
}

inline c_int32_4 operator~(const c_int32_4 &v) {
	return _mm_andnot_si128(v, _mm_set1_epi32(0xffffffff));
}

inline c_int32_4 operator+(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return _mm_add_epi32(lhs, rhs);
}

inline c_int32_4 operator-(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return _mm_sub_epi32(lhs, rhs);
}

inline c_int32_4 operator&(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return _mm_and_si128(lhs, rhs);
}

inline c_int32_4 operator|(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return _mm_or_si128(lhs, rhs);
}

inline c_int32_4 operator^(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return _mm_xor_si128(lhs, rhs);
}

inline c_int32_4 operator<<(const c_int32_4 &lhs, int32 rhs) {
	return _mm_slli_epi32(lhs, rhs);
}

inline c_int32_4 operator<<(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return _mm_sll_epi32(lhs, rhs);
}

inline c_int32_4 operator>>(const c_int32_4 &lhs, int32 rhs) {
	return _mm_srai_epi32(lhs, rhs);
}

inline c_int32_4 operator>>(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return _mm_sra_epi32(lhs, rhs);
}

inline c_int32_4 c_int32_4::shift_right_unsigned(int32 rhs) const {
	return _mm_srli_epi32(m_value, rhs);
}

inline c_int32_4 c_int32_4::shift_right_unsigned(const c_int32_4 &rhs) const {
	return _mm_srl_epi32(m_value, rhs);
}

inline c_int32_4 abs(const c_int32_4 &v) {
	return _mm_abs_epi32(v);
}

inline c_int32_4 min(const c_int32_4 &a, const c_int32_4 &b) {
	// $TODO support SSE3
	return _mm_min_epi32(a, b);
}

inline c_int32_4 max(const c_int32_4 &a, const c_int32_4 &b) {
	// $TODO support SSE3
	return _mm_max_epi32(a, b);
}

inline c_int32_4 min_unsigned(const c_int32_4 &a, const c_int32_4 &b) {
	// $TODO support SSE3
	return _mm_min_epu32(a, b);
}

inline c_int32_4 max_unsigned(const c_int32_4 &a, const c_int32_4 &b) {
	// $TODO support SSE3
	return _mm_max_epu32(a, b);
}
