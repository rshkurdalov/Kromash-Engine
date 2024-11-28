#pragma once

typedef char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;
typedef char char8;
typedef char16_t char16;
typedef char32_t char32;
typedef wchar_t wchar;
typedef unsigned char byte;
typedef int64 timestamp;
typedef float float32;
typedef double float64;

template<typename value_type> struct key
{
	value_type key_value;

	key(value_type value)
	{
		key_value = value;
	}
	
	bool operator<(const key &value) const
	{
		return key_value < value.key_value;
	}
};

template<typename value_type> struct copier
{
	void operator()(const value_type &input, value_type *output)
	{
		*output = input;
	}
};

template<typename value_type> struct mover
{
	void operator()(value_type &input, value_type *output)
	{
		*output = input;
	}
};

template<typename base_type> struct indefinite
{
	void *addr;

	indefinite() {}
	indefinite(void *addr) : addr(addr) {}
};

template<typename base_type> struct handleable
{
	indefinite<base_type> object;
	base_type *core;

	handleable() {}
	handleable(void *object, base_type *core)
		: object(object), core(core) {}
};

template<typename value_type>
constexpr value_type not_found = value_type(-1);
template<typename value_type>
constexpr value_type not_exists = value_type(-1);
