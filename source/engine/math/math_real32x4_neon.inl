inline real32x4::real32x4() {
}

inline real32x4::real32x4(real32 v)
	: m_value(vdupq_n_f32(v)) {
}

inline real32x4::real32x4(real32 a, real32 b, real32 c, real32 d) {
	// $TODO is there a more direct way?
	ALIGNAS_SIMD real32 values[] = { a, b, c, d };
	load(values);
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
	const real32 *aligned_ptr = ASSUME_ALIGNED(ptr, k_simd_alignment);
#if IS_TRUE(COMPILER_MSVC)
	m_value = vld1q_f32_ex(aligned_ptr, k_simd_alignment);
#else // IS_TRUE(COMPILER_MSVC)
	m_value = vld1q_f32(aligned_ptr);
#endif // IS_TRUE(COMPILER_MSVC)
}

inline void real32x4::load_unaligned(const real32 *ptr) {
	m_value = vld1q_f32(ptr);
}

inline void real32x4::store(real32 *ptr) const {
	wl_assert(is_pointer_aligned(ptr, k_simd_alignment));
	real32 *aligned_ptr = ASSUME_ALIGNED(ptr, k_simd_alignment);
#if IS_TRUE(COMPILER_MSVC)
	vst1q_f32_ex(aligned_ptr, m_value, k_simd_alignment);
#else // IS_TRUE(COMPILER_MSVC)
	vst1q_f32(aligned_ptr, m_value);
#endif // IS_TRUE(COMPILER_MSVC)
}

inline void real32x4::store_unaligned(real32 *ptr) {
	vst1q_f32(ptr, m_value);
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
	return vcvtq_s32_f32(m_value);
}

inline real32x4 real32x4::sum_elements() const {
	float32x2_t low_high_sum = vpadd_f32(vget_low_f32(m_value), vget_high_f32(m_value));
	float32x2_t total_sum = vpadd_f32(low_high_sum, low_high_sum);
	return vcombine_f32(total_sum, total_sum);
}

inline real32 real32x4::first_element() const {
	wl_vhalt("Not implemented"); // $TODO
	return 0.0f;
}

inline real32x4 operator+(const real32x4 &v) {
	return v;
}

inline real32x4 operator-(const real32x4 &v) {
	return vnegq_f32(v);
}

inline real32x4 operator+(const real32x4 &lhs, const real32x4 &rhs) {
	return vaddq_f32(lhs, rhs);
}

inline real32x4 operator-(const real32x4 &lhs, const real32x4 &rhs) {
	return vsubq_f32(lhs, rhs);
}

inline real32x4 operator*(const real32x4 &lhs, const real32x4 &rhs) {
	return vmulq_f32(lhs, rhs);
}

inline real32x4 operator/(const real32x4 &lhs, const real32x4 &rhs) {
	// see http://stackoverflow.com/questions/6759897/how-to-divide-in-neon-intrinsics-by-a-float-number
	t_simd_real32 reciprocal_0 = vrecpeq_f32(rhs);
	t_simd_real32 reciprocal_1 = vmulq_f32(vrecpsq_f32(rhs, reciprocal_0), reciprocal_0);
	t_simd_real32 reciprocal_2 = vmulq_f32(vrecpsq_f32(rhs, reciprocal_1), reciprocal_1);
	return vmulq_f32(lhs, reciprocal_2);
}

inline real32x4 operator%(const real32x4 &lhs, const real32x4 &rhs) {
	// Rounds toward 0
	t_simd_real32 c = vmulq_f32(lhs, vrecpeq_f32(rhs));
	t_simd_int32 i = vcvtq_s32_f32(c);
	t_simd_real32 c_trunc = vcvtq_f32_s32(i);
	t_simd_real32 base = vmulq_f32(c_trunc, rhs);
	return vsubq_f32(lhs, base);
}

inline int32x4 operator==(const real32x4 &lhs, const real32x4 &rhs) {
	return vreinterpretq_s32_u32(vceqq_f32(lhs, rhs));
}

inline int32x4 operator!=(const real32x4 &lhs, const real32x4 &rhs) {
	return vmvnq_s32(vreinterpretq_s32_u32(vceqq_f32(lhs, rhs)));
}

inline int32x4 operator>(const real32x4 &lhs, const real32x4 &rhs) {
	return vreinterpretq_s32_u32(vcgtq_f32(lhs, rhs));
}

inline int32x4 operator<(const real32x4 &lhs, const real32x4 &rhs) {
	return vreinterpretq_s32_u32(vcltq_f32(lhs, rhs));
}

inline int32x4 operator>=(const real32x4 &lhs, const real32x4 &rhs) {
	return vreinterpretq_s32_u32(vcgeq_f32(lhs, rhs));
}

inline int32x4 operator<=(const real32x4 &lhs, const real32x4 &rhs) {
	return vreinterpretq_s32_u32(vcleq_f32(lhs, rhs));
}

inline real32x4 abs(const real32x4 &v) {
	return vabsq_f32(v);
}

// Ported: http://dss.stephanierct.com/DevBlog/?p=8

inline real32x4 floor(const real32x4 &v) {
	// Algorithm: truncate rounding toward 0, subtract 1 if we rounded up
	int32x4_t v_int = vcvtq_s32_f32(v);
	float32x4_t v_trunc = vcvtq_f32_s32(v_int);
	uint32x4_t rounded_up = vcgtq_f32(v_trunc, v); // if we rounded up, fill each lane with 0xffffffff
	uint32x4_t one_uint = vreinterpretq_u32_f32(vdupq_n_f32(1.0f));
	float32x4_t one_or_zero = vreinterpretq_f32_u32(vandq_u32(one_uint, rounded_up));
	return vsubq_f32(v_trunc, one_or_zero);
}

inline real32x4 ceil(const real32x4 &v) {
	// Algorithm: truncate rounding toward 0, add 1 if we rounded down
	int32x4_t v_int = vcvtq_s32_f32(v);
	float32x4_t v_trunc = vcvtq_f32_s32(v_int);
	uint32x4_t rounded_down = vcltq_f32(v_trunc, v); // if we rounded down, fill each lane with 0xffffffff
	uint32x4_t one_uint = vreinterpretq_u32_f32(vdupq_n_f32(1.0f));
	float32x4_t one_or_zero = vreinterpretq_f32_u32(vandq_u32(one_uint, rounded_down));
	return vaddq_f32(v_trunc, one_or_zero);
}

inline real32x4 round(const real32x4 &v) {
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

inline real32x4 min(const real32x4 &a, const real32x4 &b) {
	return vminq_f32(a, b);
}

inline real32x4 max(const real32x4 &a, const real32x4 &b) {
	return vmaxq_f32(a, b);
}

inline real32x4 log(const real32x4 &v) {
	return log_ps(v);
}

inline real32x4 exp(const real32x4 &v) {
	return exp_ps(v);
}

inline real32x4 sqrt(const real32x4 &v) {
	float32x4_t rsqrt_0 = vrsqrteq_f32(v);

	float32x4_t result_1 = vrsqrtsq_f32(vmulq_f32(v, rsqrt_0), rsqrt_0);
	float32x4_t rsqrt_1 = vmulq_f32(rsqrt_0, result_1);

	float32x4_t result_2 = vrsqrtsq_f32(vmulq_f32(v, rsqrt_1), rsqrt_1);
	float32x4_t rsqrt_2 = vmulq_f32(rsqrt_1, result_2);

	return vmulq_f32(v, rsqrt_2);
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

inline void sincos(const real32x4 &v, real32x4 &sin_out, real32x4 &cos_out) {
	sincos_ps(v, reinterpret_cast<t_simd_real32 *>(&sin_out), reinterpret_cast<t_simd_real32 *>(&cos_out));
}

template<> inline int32x4 reinterpret_bits(const real32x4 &v) {
	return vreinterpretq_s32_f32(v);
}
