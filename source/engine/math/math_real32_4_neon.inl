inline c_real32_4::c_real32_4() {
}

inline c_real32_4::c_real32_4(real32 v)
	: m_value(vdupq_n_f32(v)) {
}

inline c_real32_4::c_real32_4(real32 a, real32 b, real32 c, real32 d) {
	// $TODO is there a more direct way?
	ALIGNAS_SIMD real32 values[] = { a, b, c, d };
	load(values);
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
	const real32 *aligned_ptr = ASSUME_ALIGNED(ptr, k_simd_alignment);
#if IS_TRUE(COMPILER_MSVC)
	m_value = vld1q_f32_ex(aligned_ptr, k_simd_alignment);
#else // IS_TRUE(COMPILER_MSVC)
	m_value = vld1q_f32(aligned_ptr);
#endif // IS_TRUE(COMPILER_MSVC)
}

inline void c_real32_4::store(real32 *ptr) const {
	wl_assert(is_pointer_aligned(ptr, k_simd_alignment));
	real32 *aligned_ptr = ASSUME_ALIGNED(ptr, k_simd_alignment);
#if IS_TRUE(COMPILER_MSVC)
	vst1q_f32_ex(aligned_ptr, m_value, k_simd_alignment);
#else // IS_TRUE(COMPILER_MSVC)
	vst1q_f32(aligned_ptr, m_value);
#endif // IS_TRUE(COMPILER_MSVC)
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
	return vreinterpretq_s32_f32(m_value);
}

inline c_real32_4 c_real32_4::sum_elements() const {
	float32x2_t low_high_sum = vpadd_f32(vget_low_f32(m_value), vget_high_f32(m_value));
	float32x2_t total_sum = vpadd_f32(low_high_sum, low_high_sum);
	return vcombine_f32(total_sum, total_sum);
}

inline c_real32_4 operator+(const c_real32_4 &v) {
	return v;
}

inline c_real32_4 operator-(const c_real32_4 &v) {
	return vnegq_f32(v);
}

inline c_real32_4 operator+(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return vaddq_f32(lhs, rhs);
}

inline c_real32_4 operator-(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return vsubq_f32(lhs, rhs);
}

inline c_real32_4 operator*(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return vmulq_f32(lhs, rhs);
}

inline c_real32_4 operator/(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	// see http://stackoverflow.com/questions/6759897/how-to-divide-in-neon-intrinsics-by-a-float-number
	t_simd_real32 reciprocal_0 = vrecpeq_f32(rhs);
	t_simd_real32 reciprocal_1 = vmulq_f32(vrecpsq_f32(rhs, reciprocal_0), reciprocal_0);
	t_simd_real32 reciprocal_2 = vmulq_f32(vrecpsq_f32(rhs, reciprocal_1), reciprocal_1);
	return vmulq_f32(lhs, reciprocal_2);
}

inline c_real32_4 operator%(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	// Rounds toward 0
	t_simd_real32 c = vmulq_f32(lhs, vrecpeq_f32(rhs));
	t_simd_int32 i = vcvtq_s32_f32(c);
	t_simd_real32 c_trunc = vcvtq_f32_s32(i);
	t_simd_real32 base = vmulq_f32(c_trunc, rhs);
	return vsubq_f32(lhs, base);
}

inline c_int32_4 operator==(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return vreinterpretq_s32_u32(vceqq_f32(lhs, rhs));
}

inline c_int32_4 operator!=(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return vmvnq_s32(vreinterpretq_s32_u32(vceqq_f32(lhs, rhs)));
}

inline c_int32_4 operator>(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return vreinterpretq_s32_u32(vcgtq_f32(lhs, rhs));
}

inline c_int32_4 operator<(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return vreinterpretq_s32_u32(vcltq_f32(lhs, rhs));
}

inline c_int32_4 operator>=(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return vreinterpretq_s32_u32(vcgeq_f32(lhs, rhs));
}

inline c_int32_4 operator<=(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return vreinterpretq_s32_u32(vcleq_f32(lhs, rhs));
}

inline c_real32_4 abs(const c_real32_4 &v) {
	return vabsq_f32(v);
}

// Ported: http://dss.stephanierct.com/DevBlog/?p=8

inline c_real32_4 floor(const c_real32_4 &v) {
	// Algorithm: truncate rounding toward 0, subtract 1 if we rounded up
	int32x4_t v_int = vcvtq_s32_f32(v);
	float32x4_t v_trunc = vcvtq_f32_s32(v_int);
	uint32x4_t rounded_up = vcgtq_f32(v_trunc, v); // if we rounded up, fill each lane with 0xffffffff
	uint32x4_t one_uint = vreinterpretq_u32_f32(vdupq_n_f32(1.0f));
	float32x4_t one_or_zero = vreinterpretq_f32_u32(vandq_u32(one_uint, rounded_up));
	return vsubq_f32(v_trunc, one_or_zero);
}

inline c_real32_4 ceil(const c_real32_4 &v) {
	// Algorithm: truncate rounding toward 0, add 1 if we rounded down
	int32x4_t v_int = vcvtq_s32_f32(v);
	float32x4_t v_trunc = vcvtq_f32_s32(v_int);
	uint32x4_t rounded_down = vcltq_f32(v_trunc, v); // if we rounded down, fill each lane with 0xffffffff
	uint32x4_t one_uint = vreinterpretq_u32_f32(vdupq_n_f32(1.0f));
	float32x4_t one_or_zero = vreinterpretq_f32_u32(vandq_u32(one_uint, rounded_down));
	return vaddq_f32(v_trunc, one_or_zero);
}

inline c_real32_4 round(const c_real32_4 &v) {
	// Create a vector containing the highest value less than 2
	float32x2_t two_minus_ulp_2 = vcreate_f32(0x3fffffff3fffffff);
	float32x4_t two_minus_ulp = vcombine_f32(two_minus_ulp_2, two_minus_ulp_2);

	int32x4_t v_int = vcvtq_s32_f32(v);
	float32x4_t v_trunc = vcvtq_f32_s32(v_int);
	float32x4_t remainder = vsubq_f32(v, v_trunc);

	// Compute remainder*(2 - ulp) and truncate
	float32x4_t remainder_2 = vmulq_f32(remainder, two_minus_ulp);
	int32x4_t remainder_2_int = vcvtq_s32_f32(remainder_2);
	float32x4_t remainder_2_trunc = vcvtq_f32_s32(remainder_2_int);

	return vaddq_f32(v_trunc, remainder_2_trunc);
}

inline c_real32_4 min(const c_real32_4 &a, const c_real32_4 &b) {
	return vminq_f32(a, b);
}

inline c_real32_4 max(const c_real32_4 &a, const c_real32_4 &b) {
	return vmaxq_f32(a, b);
}

inline c_real32_4 log(const c_real32_4 &v) {
	return log_ps(v);
}

inline c_real32_4 exp(const c_real32_4 &v) {
	return exp_ps(v);
}

inline c_real32_4 sqrt(const c_real32_4 &v) {
	float32x4_t rsqrt_0 = vrsqrteq_f32(v);

	float32x4_t result_1 = vrsqrtsq_f32(vmulq_f32(v, rsqrt_0), rsqrt_0);
	float32x4_t rsqrt_1 = vmulq_f32(rsqrt_0, result_1);

	float32x4_t result_2 = vrsqrtsq_f32(vmulq_f32(v, rsqrt_1), rsqrt_1);
	float32x4_t rsqrt_2 = vmulq_f32(rsqrt_1, result_2);

	return vmulq_f32(v, rsqrt_2);
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

inline c_real32_4 single_element(const c_real32_4 &v, int32 pos) {
	wl_assert(VALID_INDEX(pos, 4));
	switch (pos) {
	case 0:
		return vdupq_lane_f32(vget_low_f32(v), 0);

	case 1:
		return vdupq_lane_f32(vget_low_f32(v), 1);

	case 2:
		return vdupq_lane_f32(vget_high_f32(v), 0);

	case 3:
		return vdupq_lane_f32(vget_high_f32(v), 1);

	default:
		wl_unreachable();
		return c_real32_4(0.0f);
	}
}

template<int32 k_shift_amount>
c_real32_4 extract(const c_real32_4 &a, const c_real32_4 &b) {
	static_assert(VALID_INDEX(k_shift_amount, k_simd_block_elements), "Must be in range [0,4]");
	return vextq_f32(a, b, k_shift_amount);
}

template<>
inline c_real32_4 extract<4>(const c_real32_4 &a, const c_real32_4 &b) {
	return b;
}