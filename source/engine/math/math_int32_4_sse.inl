inline c_int32_4::c_int32_4() {
}

inline c_int32_4::c_int32_4(int32 v)
	: m_value(_mm_set1_epi32(v)) {
}

inline c_int32_4::c_int32_4(int32 a, int32 b, int32 c, int32 d)
	: m_value(_mm_set_epi32(d, c, b, a)) {
}

inline c_int32_4::c_int32_4(const int32 *ptr) {
	load(ptr);
}

inline c_int32_4::c_int32_4(const t_simd_int32 &v)
	: m_value(v) {
}

inline c_int32_4::c_int32_4(const c_int32_4 &v)
	: m_value(v) {
}

inline void c_int32_4::load(const int32 *ptr) {
	wl_assert(is_pointer_aligned(ptr, k_simd_alignment));
	m_value = _mm_load_si128(reinterpret_cast<const t_simd_int32 *>(ptr));
}

inline void c_int32_4::store(int32 *ptr) const {
	wl_assert(is_pointer_aligned(ptr, k_simd_alignment));
	_mm_store_si128(reinterpret_cast<t_simd_int32 *>(ptr), m_value);
}

inline c_int32_4 &c_int32_4::operator=(const t_simd_int32 &v) {
	m_value = v;
	return *this;
}

inline c_int32_4 &c_int32_4::operator=(const c_int32_4 &v) {
	m_value = v;
	return *this;
}

inline c_int32_4::operator t_simd_int32() const {
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
	// $TODO don't use AVX
	return _mm_sllv_epi32(lhs, rhs);
}

inline c_int32_4 operator>>(const c_int32_4 &lhs, int32 rhs) {
	return _mm_srai_epi32(lhs, rhs);
}

inline c_int32_4 operator>>(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	// $TODO don't use AVX
	return _mm_srav_epi32(lhs, rhs);
}

inline c_int32_4 c_int32_4::shift_right_unsigned(int32 rhs) const {
	return _mm_srli_epi32(m_value, rhs);
}

inline c_int32_4 c_int32_4::shift_right_unsigned(const c_int32_4 &rhs) const {
	// $TODO don't use AVX
	return _mm_srlv_epi32(m_value, rhs);
}

inline c_int32_4 operator==(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return _mm_cmpeq_epi32(lhs, rhs);
}

inline c_int32_4 operator!=(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return ~c_int32_4(_mm_cmpeq_epi32(lhs, rhs));
}

inline c_int32_4 operator>(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return _mm_cmpgt_epi32(lhs, rhs);
}

inline c_int32_4 operator<(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return _mm_cmplt_epi32(lhs, rhs);
}

inline c_int32_4 operator>=(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return ~c_int32_4(_mm_cmplt_epi32(lhs, rhs));
}

inline c_int32_4 operator<=(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return ~c_int32_4(_mm_cmpgt_epi32(lhs, rhs));
}

inline c_int32_4 greater_unsigned(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return ~less_equal_unsigned(lhs, rhs);
}

inline c_int32_4 less_unsigned(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return ~greater_equal_unsigned(lhs, rhs);
}

inline c_int32_4 greater_equal_unsigned(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return _mm_cmpeq_epi32(lhs, _mm_max_epu32(lhs, rhs)); // (a == max(a,b)) == (a >= b)
}

inline c_int32_4 less_equal_unsigned(const c_int32_4 &lhs, const c_int32_4 &rhs) {
	return _mm_cmpeq_epi32(lhs, _mm_min_epu32(lhs, rhs)); // (a == min(a,b)) == (a <= b)
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

inline c_real32_4 convert_to_real32_4(const c_int32_4 &v) {
	return _mm_cvtepi32_ps(v);
}

inline int32 mask_from_msb(const c_int32_4 &v) {
	return _mm_movemask_ps(v.real32_4_from_bits());
}

inline c_int32_4 single_element(const c_int32_4 &v, int32 pos) {
	wl_assert(VALID_INDEX(pos, 4));
	switch (pos) {
	case 0:
		return _mm_shuffle_epi32(v, 0);

	case 1:
		return _mm_shuffle_epi32(v, (1 | (1 << 2) | (1 << 4) | (1 << 6)));

	case 2:
		return _mm_shuffle_epi32(v, (2 | (2 << 2) | (2 << 4) | (2 << 6)));

	case 3:
		return _mm_shuffle_epi32(v, (3 | (3 << 2) | (3 << 4) | (3 << 6)));

	default:
		wl_unreachable();
		return c_int32_4(0);
	}
}

template<int32 k_pos_0, int32 k_pos_1, int32 k_pos_2, int32 k_pos_3>
c_int32_4 shuffle(const c_int32_4 &v) {
	static_assert(VALID_INDEX(k_pos_0, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_1, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_2, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_3, k_simd_block_elements), "Must be in range [0,3]");
	static const int32 k_shuffle_pos = k_pos_0 | (k_pos_1 << 2) | (k_pos_2 << 4) | (k_pos_3 << 6);
	return _mm_shuffle_epi32(v, k_shuffle_pos);
}

template<int32 k_pos_0, int32 k_pos_1, int32 k_pos_2, int32 k_pos_3>
c_int32_4 shuffle(const c_int32_4 &a, const c_int32_4 &b) {
	static_assert(VALID_INDEX(k_pos_0, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_1, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_2, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_3, k_simd_block_elements), "Must be in range [0,3]");
	static const int32 k_shuffle_pos = k_pos_0 | (k_pos_1 << 2) | (k_pos_2 << 4) | (k_pos_3 << 6);
	return c_real32_4(_mm_shuffle_ps(
		a.real32_4_from_bits(), b.real32_4_from_bits(), k_shuffle_pos)).int32_4_from_bits();
}

template<>
inline c_int32_4 extract<0>(const c_int32_4 &a, const c_int32_4 &b) {
	return a;
}

template<>
inline c_int32_4 extract<1>(const c_int32_4 &a, const c_int32_4 &b) {
	// [.xyz][w...] => [z.w.]
	// [.xyz][z.w.] => [xyzw]
	c_int32_4 c = shuffle<3, 0, 0, 0>(a, b);
	return shuffle<1, 2, 0, 2>(a, c);
}

template<>
inline c_int32_4 extract<2>(const c_int32_4 &a, const c_int32_4 &b) {
	// [..xy][zw..] => [xyzw]
	return shuffle<2, 3, 0, 1>(a, b);
}

template<>
inline c_int32_4 extract<3>(const c_int32_4 &a, const c_int32_4 &b) {
	// [...x][yzw.] => [x.y.]
	// [x.y.][yzw.] => [xyzw]
	c_int32_4 c = shuffle<3, 0, 0, 0>(a, b);
	return shuffle<0, 2, 1, 2>(c, b);
}

template<>
inline c_int32_4 extract<4>(const c_int32_4 &a, const c_int32_4 &b) {
	return b;
}
