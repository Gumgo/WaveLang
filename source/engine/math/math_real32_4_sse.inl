inline c_real32_4::c_real32_4() {
}

inline c_real32_4::c_real32_4(real32 v)
	: m_value(_mm_set1_ps(v)) {
}

inline c_real32_4::c_real32_4(real32 a, real32 b, real32 c, real32 d)
	: m_value(_mm_set_ps(d, c, b, a)) {
}

inline c_real32_4::c_real32_4(const real32 *ptr) {
	load(ptr);
}

inline c_real32_4::c_real32_4(const t_simd_real32 &v)
	: m_value(v) {
}

inline c_real32_4::c_real32_4(const c_real32_4 &v)
	: m_value(v.m_value) {
}

inline void c_real32_4::load(const real32 *ptr) {
	wl_assert(is_pointer_aligned(ptr, k_simd_alignment));
	m_value = _mm_load_ps(ptr);
}

inline void c_real32_4::store(real32 *ptr) const {
	wl_assert(is_pointer_aligned(ptr, k_simd_alignment));
	_mm_store_ps(ptr, m_value);
}

inline c_real32_4 &c_real32_4::c_real32_4::operator=(const t_simd_real32 &v) {
	m_value = v;
	return *this;
}

inline c_real32_4 &c_real32_4::operator=(const c_real32_4 &v) {
	m_value = v.m_value;
	return *this;
}

inline c_real32_4::operator t_simd_real32() const {
	return m_value;
}

inline c_int32_4 c_real32_4::int32_4_from_bits() const {
	return _mm_castps_si128(m_value);
}

inline c_real32_4 c_real32_4::sum_elements() const {
	c_real32_4 x = _mm_hadd_ps(m_value, m_value);
	x = _mm_hadd_ps(x, x);
	return x;
}

inline c_real32_4 operator+(const c_real32_4 &v) {
	return v;
}

inline c_real32_4 operator-(const c_real32_4 &v) {
	return _mm_sub_ps(_mm_set1_ps(0.0f), v);
}

inline c_real32_4 operator+(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return _mm_add_ps(lhs, rhs);
}

inline c_real32_4 operator-(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return _mm_sub_ps(lhs, rhs);
}

inline c_real32_4 operator*(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return _mm_mul_ps(lhs, rhs);
}

inline c_real32_4 operator/(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return _mm_div_ps(lhs, rhs);
}

inline c_real32_4 operator%(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	// Rounds toward 0
	t_simd_real32 c = _mm_div_ps(lhs, rhs);
	t_simd_int32 i = _mm_cvttps_epi32(c);
	t_simd_real32 c_trunc = _mm_cvtepi32_ps(i);
	t_simd_real32 base = _mm_mul_ps(c_trunc, rhs);
	return _mm_sub_ps(lhs, base);
}

inline c_int32_4 operator==(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return c_real32_4(_mm_cmpeq_ps(lhs, rhs)).int32_4_from_bits();
}

inline c_int32_4 operator!=(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return c_real32_4(_mm_cmpneq_ps(lhs, rhs)).int32_4_from_bits();
}

inline c_int32_4 operator>(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return c_real32_4(_mm_cmpgt_ps(lhs, rhs)).int32_4_from_bits();
}

inline c_int32_4 operator<(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return c_real32_4(_mm_cmplt_ps(lhs, rhs)).int32_4_from_bits();
}

inline c_int32_4 operator>=(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return c_real32_4(_mm_cmpge_ps(lhs, rhs)).int32_4_from_bits();
}

inline c_int32_4 operator<=(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return c_real32_4(_mm_cmple_ps(lhs, rhs)).int32_4_from_bits();
}

inline c_real32_4 abs(const c_real32_4 &v) {
	// Mask off the sign bit for fast abs
	static const t_simd_real32 k_sign_mask = _mm_set1_ps(-0.0f);
	return _mm_andnot_ps(k_sign_mask, v);
}

inline c_real32_4 floor(const c_real32_4 &v) {
	// $TODO support SSE3
	return _mm_floor_ps(v);
}

inline c_real32_4 ceil(const c_real32_4 &v) {
	// $TODO support SSE3
	return _mm_ceil_ps(v);
}

inline c_real32_4 round(const c_real32_4 &v) {
	// $TODO support SSE3
	return _mm_round_ps(v, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
}

inline c_real32_4 min(const c_real32_4 &a, const c_real32_4 &b) {
	return _mm_min_ps(a, b);
}

inline c_real32_4 max(const c_real32_4 &a, const c_real32_4 &b) {
	return _mm_max_ps(a, b);
}

inline c_real32_4 log(const c_real32_4 &v) {
	return log_ps(v);
}

inline c_real32_4 exp(const c_real32_4 &v) {
	return exp_ps(v);
}

inline c_real32_4 sqrt(const c_real32_4 &v) {
	return _mm_sqrt_ps(v);
}

inline c_real32_4 pow(const c_real32_4 &a, const c_real32_4 &b) {
	return exp(b * log(a));
}

inline c_real32_4 sin(const c_real32_4 &v) {
	return sin_ps(v);
}

inline c_real32_4 cos(const c_real32_4 &v) {
	return cos_ps(v);
}

inline void sincos(const c_real32_4 &v, c_real32_4 &out_sin, c_real32_4 &out_cos) {
	sincos_ps(v, reinterpret_cast<t_simd_real32 *>(&out_sin), reinterpret_cast<t_simd_real32 *>(&out_cos));
}

inline c_int32_4 convert_to_int32_4(const c_real32_4 &v) {
	return _mm_cvtps_epi32(v);
}

inline c_real32_4 single_element(const c_real32_4 &v, int32 pos) {
	wl_assert(VALID_INDEX(pos, 4));
	switch (pos) {
	case 0:
		return c_int32_4(_mm_shuffle_epi32(
			v.int32_4_from_bits(), 0)).real32_4_from_bits();

	case 1:
		return c_int32_4(_mm_shuffle_epi32(
			v.int32_4_from_bits(), (1 | (1 << 2) | (1 << 4) | (1 << 6)))).real32_4_from_bits();

	case 2:
		return c_int32_4(_mm_shuffle_epi32(
			v.int32_4_from_bits(), (2 | (2 << 2) | (2 << 4) | (2 << 6)))).real32_4_from_bits();

	case 3:
		return c_int32_4(_mm_shuffle_epi32(
			v.int32_4_from_bits(), (3 | (3 << 2) | (3 << 4) | (3 << 6)))).real32_4_from_bits();

	default:
		wl_unreachable();
		return c_real32_4(0.0f);
	}
}

template<int32 k_pos_0, int32 k_pos_1, int32 k_pos_2, int32 k_pos_3>
c_real32_4 shuffle(const c_real32_4 &v) {
	static_assert(VALID_INDEX(k_pos_0, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_1, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_2, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_3, k_simd_block_elements), "Must be in range [0,3]");
	static const int32 k_shuffle_pos = k_pos_0 | (k_pos_1 << 2) | (k_pos_2 << 4) | (k_pos_3 << 6);
	return c_int32_4(_mm_shuffle_epi32(v.int32_4_from_bits(), k_shuffle_pos)).real32_4_from_bits();
}

template<int32 k_pos_0, int32 k_pos_1, int32 k_pos_2, int32 k_pos_3>
c_real32_4 shuffle(const c_real32_4 &a, const c_real32_4 &b) {
	static_assert(VALID_INDEX(k_pos_0, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_1, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_2, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_3, k_simd_block_elements), "Must be in range [0,3]");
	static const int32 k_shuffle_pos = k_pos_0 | (k_pos_1 << 2) | (k_pos_2 << 4) | (k_pos_3 << 6);
	return _mm_shuffle_ps(a, b, k_shuffle_pos);
}

template<>
inline c_real32_4 extract<0>(const c_real32_4 &a, const c_real32_4 &b) {
	return a;
}

template<>
inline c_real32_4 extract<1>(const c_real32_4 &a, const c_real32_4 &b) {
	// [.xyz][w...] => [z.w.]
	// [.xyz][z.w.] => [xyzw]
	c_real32_4 c = shuffle<3, 0, 0, 0>(a, b);
	return shuffle<1, 2, 0, 2>(a, c);
}

template<>
inline c_real32_4 extract<2>(const c_real32_4 &a, const c_real32_4 &b) {
	// [..xy][zw..] => [xyzw]
	return shuffle<2, 3, 0, 1>(a, b);
}

template<>
inline c_real32_4 extract<3>(const c_real32_4 &a, const c_real32_4 &b) {
	// [...x][yzw.] => [x.y.]
	// [x.y.][yzw.] => [xyzw]
	c_real32_4 c = shuffle<3, 0, 0, 0>(a, b);
	return shuffle<0, 2, 1, 2>(c, b);
}

template<>
inline c_real32_4 extract<4>(const c_real32_4 &a, const c_real32_4 &b) {
	return b;
}
