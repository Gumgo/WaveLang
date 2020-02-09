inline real32x4::real32x4() {
}

inline real32x4::real32x4(real32 v)
	: m_value(_mm_set1_ps(v)) {
}

inline real32x4::real32x4(real32 a, real32 b, real32 c, real32 d)
	: m_value(_mm_set_ps(d, c, b, a)) {
}

inline real32x4::real32x4(const real32 *ptr) {
	load(ptr);
}

inline real32x4::real32x4(const t_simd_real32 &v)
	: m_value(v) {
}

inline real32x4::real32x4(const real32x4 &v)
	: m_value(v.m_value) {
}

inline void real32x4::load(const real32 *ptr) {
	wl_assert(is_pointer_aligned(ptr, k_simd_alignment));
	m_value = _mm_load_ps(ptr);
}

inline void real32x4::store(real32 *ptr) const {
	wl_assert(is_pointer_aligned(ptr, k_simd_alignment));
	_mm_store_ps(ptr, m_value);
}

inline real32x4 &real32x4::real32x4::operator=(const t_simd_real32 &v) {
	m_value = v;
	return *this;
}

inline real32x4 &real32x4::operator=(const real32x4 &v) {
	m_value = v.m_value;
	return *this;
}

inline real32x4::operator t_simd_real32() const {
	return m_value;
}

inline real32x4::operator int32x4() const {
	return _mm_cvtps_epi32(m_value);
}

inline real32x4 real32x4::sum_elements() const {
	real32x4 x = _mm_hadd_ps(m_value, m_value);
	x = _mm_hadd_ps(x, x);
	return x;
}

inline real32x4 operator+(const real32x4 &v) {
	return v;
}

inline real32x4 operator-(const real32x4 &v) {
	return _mm_sub_ps(_mm_set1_ps(0.0f), v);
}

inline real32x4 operator+(const real32x4 &lhs, const real32x4 &rhs) {
	return _mm_add_ps(lhs, rhs);
}

inline real32x4 operator-(const real32x4 &lhs, const real32x4 &rhs) {
	return _mm_sub_ps(lhs, rhs);
}

inline real32x4 operator*(const real32x4 &lhs, const real32x4 &rhs) {
	return _mm_mul_ps(lhs, rhs);
}

inline real32x4 operator/(const real32x4 &lhs, const real32x4 &rhs) {
	return _mm_div_ps(lhs, rhs);
}

inline real32x4 operator%(const real32x4 &lhs, const real32x4 &rhs) {
	// Rounds toward 0
	t_simd_real32 c = _mm_div_ps(lhs, rhs);
	t_simd_int32 i = _mm_cvttps_epi32(c);
	t_simd_real32 c_trunc = _mm_cvtepi32_ps(i);
	t_simd_real32 base = _mm_mul_ps(c_trunc, rhs);
	return _mm_sub_ps(lhs, base);
}

inline int32x4 operator==(const real32x4 &lhs, const real32x4 &rhs) {
	return reinterpret_bits<int32x4>(real32x4(_mm_cmpeq_ps(lhs, rhs)));
}

inline int32x4 operator!=(const real32x4 &lhs, const real32x4 &rhs) {
	return reinterpret_bits<int32x4>(real32x4(_mm_cmpneq_ps(lhs, rhs)));
}

inline int32x4 operator>(const real32x4 &lhs, const real32x4 &rhs) {
	return reinterpret_bits<int32x4>(real32x4(_mm_cmpgt_ps(lhs, rhs)));
}

inline int32x4 operator<(const real32x4 &lhs, const real32x4 &rhs) {
	return reinterpret_bits<int32x4>(real32x4(_mm_cmplt_ps(lhs, rhs)));
}

inline int32x4 operator>=(const real32x4 &lhs, const real32x4 &rhs) {
	return reinterpret_bits<int32x4>(real32x4(_mm_cmpge_ps(lhs, rhs)));
}

inline int32x4 operator<=(const real32x4 &lhs, const real32x4 &rhs) {
	return reinterpret_bits<int32x4>(real32x4(_mm_cmple_ps(lhs, rhs)));
}

inline real32x4 abs(const real32x4 &v) {
	// Mask off the sign bit for fast abs
	static const t_simd_real32 k_sign_mask = _mm_set1_ps(-0.0f);
	return _mm_andnot_ps(k_sign_mask, v);
}

inline real32x4 floor(const real32x4 &v) {
	// $TODO support SSE3
	return _mm_floor_ps(v);
}

inline real32x4 ceil(const real32x4 &v) {
	// $TODO support SSE3
	return _mm_ceil_ps(v);
}

inline real32x4 round(const real32x4 &v) {
	// $TODO support SSE3
	return _mm_round_ps(v, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
}

inline real32x4 min(const real32x4 &a, const real32x4 &b) {
	return _mm_min_ps(a, b);
}

inline real32x4 max(const real32x4 &a, const real32x4 &b) {
	return _mm_max_ps(a, b);
}

inline real32x4 log(const real32x4 &v) {
	return log_ps(v);
}

inline real32x4 exp(const real32x4 &v) {
	return exp_ps(v);
}

inline real32x4 sqrt(const real32x4 &v) {
	return _mm_sqrt_ps(v);
}

inline real32x4 pow(const real32x4 &a, const real32x4 &b) {
	return exp(b * log(a));
}

inline real32x4 sin(const real32x4 &v) {
	return sin_ps(v);
}

inline real32x4 cos(const real32x4 &v) {
	return cos_ps(v);
}

inline void sincos(const real32x4 &v, real32x4 &out_sin, real32x4 &out_cos) {
	sincos_ps(v, reinterpret_cast<t_simd_real32 *>(&out_sin), reinterpret_cast<t_simd_real32 *>(&out_cos));
}

template<> inline int32x4 reinterpret_bits(const real32x4 &v) {
	return _mm_castps_si128(v);
}

inline real32x4 single_element(const real32x4 &v, int32 pos) {
	wl_assert(VALID_INDEX(pos, 4));
	switch (pos) {
	case 0:
		return reinterpret_bits<real32x4>(
			int32x4(_mm_shuffle_epi32(reinterpret_bits<int32x4>(v), 0)));

	case 1:
		return reinterpret_bits<real32x4>(
			int32x4(_mm_shuffle_epi32(reinterpret_bits<int32x4>(v), (1 | (1 << 2) | (1 << 4) | (1 << 6)))));

	case 2:
		return reinterpret_bits<real32x4>(
			int32x4(_mm_shuffle_epi32(reinterpret_bits<int32x4>(v), (2 | (2 << 2) | (2 << 4) | (2 << 6)))));

	case 3:
		return reinterpret_bits<real32x4>(
			int32x4(_mm_shuffle_epi32(reinterpret_bits<int32x4>(v), (3 | (3 << 2) | (3 << 4) | (3 << 6)))));

	default:
		wl_unreachable();
		return real32x4(0.0f);
	}
}

template<int32 k_pos_0, int32 k_pos_1, int32 k_pos_2, int32 k_pos_3>
real32x4 shuffle(const real32x4 &v) {
	static_assert(VALID_INDEX(k_pos_0, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_1, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_2, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_3, k_simd_block_elements), "Must be in range [0,3]");
	static const int32 k_shuffle_pos = k_pos_0 | (k_pos_1 << 2) | (k_pos_2 << 4) | (k_pos_3 << 6);
	return reinterpret_bits<real32x4>(int32x4(_mm_shuffle_epi32(reinterpret_bits<int32x4>(v), k_shuffle_pos)));
}

template<int32 k_pos_0, int32 k_pos_1, int32 k_pos_2, int32 k_pos_3>
real32x4 shuffle(const real32x4 &a, const real32x4 &b) {
	static_assert(VALID_INDEX(k_pos_0, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_1, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_2, k_simd_block_elements), "Must be in range [0,3]");
	static_assert(VALID_INDEX(k_pos_3, k_simd_block_elements), "Must be in range [0,3]");
	static const int32 k_shuffle_pos = k_pos_0 | (k_pos_1 << 2) | (k_pos_2 << 4) | (k_pos_3 << 6);
	return _mm_shuffle_ps(a, b, k_shuffle_pos);
}

template<>
inline real32x4 extract<0>(const real32x4 &a, const real32x4 &b) {
	return a;
}

template<>
inline real32x4 extract<1>(const real32x4 &a, const real32x4 &b) {
	// [.xyz][w...] => [z.w.]
	// [.xyz][z.w.] => [xyzw]
	real32x4 c = shuffle<3, 0, 0, 0>(a, b);
	return shuffle<1, 2, 0, 2>(a, c);
}

template<>
inline real32x4 extract<2>(const real32x4 &a, const real32x4 &b) {
	// [..xy][zw..] => [xyzw]
	return shuffle<2, 3, 0, 1>(a, b);
}

template<>
inline real32x4 extract<3>(const real32x4 &a, const real32x4 &b) {
	// [...x][yzw.] => [x.y.]
	// [x.y.][yzw.] => [xyzw]
	real32x4 c = shuffle<3, 0, 0, 0>(a, b);
	return shuffle<0, 2, 1, 2>(c, b);
}

template<>
inline real32x4 extract<4>(const real32x4 &a, const real32x4 &b) {
	return b;
}
