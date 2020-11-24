#pragma once

#include <type_traits>

template<typename t_enum>
constexpr std::underlying_type_t<t_enum> enum_index(t_enum value) {
	return static_cast<std::underlying_type_t<t_enum>>(value);
}

template<typename t_enum>
constexpr std::underlying_type_t<t_enum> enum_count() {
	return static_cast<std::underlying_type_t<t_enum>>(t_enum::k_count);
}

template<typename t_enum>
constexpr bool valid_enum_index(t_enum value) {
	return enum_index(value) >= 0 && enum_index(value) < enum_count<t_enum>();
}

template<typename t_enum>
constexpr t_enum enum_next(t_enum value) {
	return static_cast<t_enum>(static_cast<std::underlying_type_t<t_enum>>(value + 1));
}

template<typename t_enum>
class c_enum_iterator {
public:
	c_enum_iterator()
		: m_index(0) {}

	bool is_valid() const {
		return m_index < enum_count<t_enum>();
	}

	void next() {
		m_index++;
	}

	t_enum get() const {
		wl_assert(is_valid());
		return static_cast<t_enum>(m_index);
	}

	class c_iterand {
	public:
		c_iterand(std::underlying_type_t<t_enum> index)
			: m_index(index) {}

		bool operator!=(const c_iterand &other) {
			return m_index != other.m_index;
		}

		c_iterand &operator++() {
			m_index++;
			return *this;
		}

		t_enum operator*() const {
			return static_cast<t_enum>(m_index);
		}

	private:
		std::underlying_type_t<t_enum> m_index;
	};

	c_iterand begin() const {
		return c_iterand(0);
	}

	c_iterand end() const {
		return c_iterand(enum_count<t_enum>());
	}

private:
	std::underlying_type_t<t_enum> m_index;
};

template<typename t_enum>
c_enum_iterator<t_enum> iterate_enum() {
	return c_enum_iterator<t_enum>();
}

template<size_t k_count>
struct s_enum_flags_underlying_type {
	static_assert(k_count <= 64, "Too many enum values to support s_enum_flags");
	// Optimization: we could avoid some template recursion by snapping to the previous power of 2
	using t_underlying_type = typename s_enum_flags_underlying_type<k_count - 1>::t_underlying_type;
};

template<>
struct s_enum_flags_underlying_type<0> {
	using t_underlying_type = uint8;
};

template<>
struct s_enum_flags_underlying_type<9> {
	using t_underlying_type = uint16;
};

template<>
struct s_enum_flags_underlying_type<17> {
	using t_underlying_type = uint32;
};

template<>
struct s_enum_flags_underlying_type<33> {
	using t_underlying_type = uint64;
};

template<typename t_enum>
struct s_enum_flags {
	using t_underlying_type = typename s_enum_flags_underlying_type<t_enum::k_count>::t_underlying_type;
	t_underlying_type flags;

	constexpr s_enum_flags() = default;

	constexpr s_enum_flags(t_enum value) {
		flags = static_cast<t_underlying_type>(1) << static_cast<t_underlying_type>(value);
	}

	static constexpr s_enum_flags<t_enum> empty() {
		return s_enum_flags<t_enum> { 0 };
	}

	constexpr bool test_flag(t_enum value) const {
		return (flags | (static_cast<t_underlying_type>(1) << static_cast<t_underlying_type>(value))) != 0;
	}

	constexpr s_enum_flags<t_enum> operator|(const s_enum_flags<t_enum> &flag) const {
		return { flags | flag.flags };
	}

	bool operator==(const s_enum_flags<t_enum> &other) const {
		return flags == other.flags;
	}

	bool operator!=(const s_enum_flags<t_enum> &other) const {
		return flags != other.flags;
	}
};

template<typename t_enum>
constexpr s_enum_flags<t_enum> enum_flag(t_enum value) {
	return { 1 << static_cast<typename s_enum_flags<t_enum>::t_underlying_type>(value) };
}
