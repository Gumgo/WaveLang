inline int32x4::int32x4() {
}

inline int32x4::int32x4(int32 v)
	: m_value(vdupq_n_s32(v)) {
}

inline int32x4::int32x4(int32 a, int32 b, int32 c, int32 d) {
	// $TODO is there a more direct way?
	ALIGNAS_SIMD int32 values[] = { a, b, c, d };
	load(values);
}

inline int32x4::int32x4(const int32 *ptr) {
	load(ptr);
}

inline int32x4::int32x4(const t_simd_int32 &v)
	: m_value(v) {
}

inline int32x4::int32x4(const int32x4 &v)
	: m_value(v) {
}

inline void int32x4::load(const int32 *ptr) {
	wl_assert(is_pointer_aligned(ptr, k_simd_alignment));
	const int32 *aligned_ptr = ASSUME_ALIGNED(ptr, k_simd_alignment);
#if IS_TRUE(COMPILER_MSVC)
	m_value = vld1q_s32_ex(aligned_ptr, k_simd_alignment);
#else // IS_TRUE(COMPILER_MSVC)
	m_value = vld1q_s32(aligned_ptr);
#endif // IS_TRUE(COMPILER_MSVC)
}

inline void int32x4::store(int32 *ptr) const {
	wl_assert(is_pointer_aligned(ptr, k_simd_alignment));
	int32 *aligned_ptr = ASSUME_ALIGNED(ptr, k_simd_alignment);
#if IS_TRUE(COMPILER_MSVC)
	vst1q_s32_ex(aligned_ptr, m_value, k_simd_alignment);
#else // IS_TRUE(COMPILER_MSVC)
	vst1q_s32(aligned_ptr, m_value);
#endif // IS_TRUE(COMPILER_MSVC)
}

inline int32x4 &int32x4::operator=(const t_simd_int32 &v) {
	m_value = v;
	return *this;
}

inline int32x4 &int32x4::operator=(const int32x4 &v) {
	m_value = v;
	return *this;
}

inline int32x4::operator t_simd_int32() const {
	return m_value;
}

inline int32x4::operator real32x4() const {
	return vcvtq_f32_s32(m_value);
}

inline real32x4 real32x4::sum_elements() const {
	int32x2_t low_high_sum = vpadd_s32(vget_low_s32(m_value), vget_high_s32(m_value));
	int32x2_t total_sum = vpadd_s32(low_high_sum, low_high_sum);
	return vcombine_s32(total_sum, total_sum);
}

inline int32 int32x4::first_element() const {
	wl_vhalt("Not implemented"); // $TODO
	return 0;
}

inline int32x4 operator+(const int32x4 &v) {
	return v;
}

inline int32x4 operator-(const int32x4 &v) {
	return vnegq_s32(v);
}

inline int32x4 operator~(const int32x4 &v) {
	return vmvnq_s32(v);
}

inline int32x4 operator+(const int32x4 &lhs, const int32x4 &rhs) {
	return vaddq_s32(lhs, rhs);
}

inline int32x4 operator-(const int32x4 &lhs, const int32x4 &rhs) {
	return vsubq_s32(lhs, rhs);
}

inline int32x4 operator-(const int32x4 &lhs, const int32x4 &rhs) {
	return vmulq_s32(lhs, rhs);
}

inline int32x4 operator&(const int32x4 &lhs, const int32x4 &rhs) {
	return vandq_s32(lhs, rhs);
}

inline int32x4 operator|(const int32x4 &lhs, const int32x4 &rhs) {
	return vorrq_s32(lhs, rhs);
}

inline int32x4 operator^(const int32x4 &lhs, const int32x4 &rhs) {
	return veorq_s32(lhs, rhs);
}

inline int32x4 operator<<(const int32x4 &lhs, int32 rhs) {
	return vshlq_s32(lhs, vdupq_n_s32(rhs));
}

inline int32x4 operator<<(const int32x4 &lhs, const int32x4 &rhs) {
	return vshlq_s32(lhs, rhs);
}

inline int32x4 operator>>(const int32x4 &lhs, int32 rhs) {
	return vshlq_s32(lhs, vnegq_s32(vdupq_n_s32(rhs)));
}

inline int32x4 operator>>(const int32x4 &lhs, const int32x4 &rhs) {
	return vshlq_s32(lhs, vnegq_s32(rhs));
}

inline int32x4 int32x4::shift_right_unsigned(int32 rhs) const {
	return vreinterpretq_s32_u32(vshlq_u32(vreinterpretq_u32_s32(m_value), vnegq_s32(vdupq_n_s32(rhs))));
}

inline int32x4 int32x4::shift_right_unsigned(const int32x4 &rhs) const {
	return vreinterpretq_s32_u32(vshlq_u32(vreinterpretq_u32_s32(m_value), vnegq_s32(rhs)));
}

inline int32x4 operator==(const int32x4 &lhs, const int32x4 &rhs) {
	return vreinterpretq_s32_u32(vceqq_s32(lhs, rhs));
}

inline int32x4 operator!=(const int32x4 &lhs, const int32x4 &rhs) {
	return vmvnq_s32(vreinterpretq_s32_u32(vceqq_s32(lhs, rhs)));
}

inline int32x4 operator>(const int32x4 &lhs, const int32x4 &rhs) {
	return vreinterpretq_s32_u32(vcgtq_s32(lhs, rhs));
}

inline int32x4 operator<(const int32x4 &lhs, const int32x4 &rhs) {
	return vreinterpretq_s32_u32(vcltq_s32(lhs, rhs));
}

inline int32x4 operator>=(const int32x4 &lhs, const int32x4 &rhs) {
	return vreinterpretq_s32_u32(vcgeq_s32(lhs, rhs));
}

inline int32x4 operator<=(const int32x4 &lhs, const int32x4 &rhs) {
	return vreinterpretq_s32_u32(vcleq_s32(lhs, rhs));
}

inline int32x4 greater_unsigned(const int32x4 &lhs, const int32x4 &rhs) {
	return vreinterpretq_s32_u32(vcgtq_u32(vreinterpretq_u32_s32(lhs), vreinterpretq_u32_s32(rhs)));
}

inline int32x4 less_unsigned(const int32x4 &lhs, const int32x4 &rhs) {
	return vreinterpretq_s32_u32(vcltq_u32(vreinterpretq_u32_s32(lhs), vreinterpretq_u32_s32(rhs)));
}

inline int32x4 greater_equal_unsigned(const int32x4 &lhs, const int32x4 &rhs) {
	return vreinterpretq_s32_u32(vcgeq_u32(vreinterpretq_u32_s32(lhs), vreinterpretq_u32_s32(rhs)));
}

inline int32x4 less_equal_unsigned(const int32x4 &lhs, const int32x4 &rhs) {
	return vreinterpretq_s32_u32(vcleq_u32(vreinterpretq_u32_s32(lhs), vreinterpretq_u32_s32(rhs)));
}

inline int32x4 abs(const int32x4 &v) {
	return vabsq_s32(v);
}

inline int32x4 min(const int32x4 &a, const int32x4 &b) {
	return vminq_s32(a, b);
}

inline int32x4 max(const int32x4 &a, const int32x4 &b) {
	return vmaxq_s32(a, b);
}

inline int32x4 min_unsigned(const int32x4 &a, const int32x4 &b) {
	return vreinterpretq_s32_u32(vminq_u32(vreinterpretq_u32_s32(a), vreinterpretq_u32_s32(b)));
}

inline int32x4 max_unsigned(const int32x4 &a, const int32x4 &b) {
	return vreinterpretq_s32_u32(vmaxq_u32(vreinterpretq_u32_s32(a), vreinterpretq_u32_s32(b)));
}

template<> inline real32x4 reinterpret_bits(const int32x4 &v) {
	return vreinterpretq_f32_s32(v);
}

inline int32 mask_from_msb(const int32x4 &v) {
	// AND all bits but the MSB
	int32x4_t msb = vandq_s32(v, vdupq_n_s32(0x80000000));

	// Shift bits into positions
	int32x4_t shift_amount = vcombine_s32(
		vcreate_s32(0xffffffe2ffffffe1ull), // -31, -30
		vcreate_s32(0xffffffe4ffffffe3ull)); // -29, -28
	int32x4_t msb_shifted = vreinterpretq_s32_u32(vshlq_u32(vreinterpretq_u32_s32(msb), shift_amount));

	// OR all lanes together - since the lanes are all disjoint bits, we can just do horizontal adds
	int32x2_t low_high_sum = vpadd_s32(vget_low_s32(msb_shifted), vget_high_s32(msb_shifted));
	int32x2_t total_sum = vpadd_s32(low_high_sum, low_high_sum);
	return vget_lane_s32(total_sum, 0);
}

inline bool all_true(const int32x4 &v) {
	int32x2_t low_high = vand_s32(vget_low_s32(v), vget_high_s32(v));
	return (vget_lane_s32(low_high, 0) & vget_lane_s32(low_high, 1)) != 0;
}

inline bool all_false(const int32x4 &v) {
	int32x2_t low_high = vor_s32(vget_low_s32(v), vget_high_s32(v));
	return (vget_lane_s32(low_high, 0) | vget_lane_s32(low_high, 1)) == 0;
}

inline bool any_true(const int32x4 &v) {
	int32x2_t low_high = vor_s32(vget_low_s32(v), vget_high_s32(v));
	return (vget_lane_s32(low_high, 0) | vget_lane_s32(low_high, 1)) != 0;
}

inline bool any_false(const int32x4 &v) {
	int32x2_t low_high = vand_s32(vget_low_s32(v), vget_high_s32(v));
	return (vget_lane_s32(low_high, 0) & vget_lane_s32(low_high, 1)) == 0;
}
