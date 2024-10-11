#pragma once
#include "real.h"

struct string_mapping_config //!!!
{
	uint64 min_fraction_digits;
	uint64 max_fraction_digits;

	string_mapping_config();
};

string_mapping_config *string_config();

template<typename char_type> struct string
{
	/*If array is empty addr is nullptr*/
	char_type *addr;
	/*If addr is nullptr - size must be 0*/
	uint64 size;
	/*If addr is nullptr - capacity must be 0*/
	uint64 capacity;

	string()
	{
		addr = nullptr;
		size = 0;
		capacity = 0;
	}

	string(const char_type *source)
	{
		addr = nullptr;
		size = 0;
		capacity = 0;
		*this << source;
	}

	~string()
	{
		if(addr != nullptr) delete[] addr;
	}

	/*Increases capacity by a minimum of min_value elements*/
	void increase_capacity(uint64 min_value)
	{
		min_value += capacity;
		if(capacity == 0) capacity = 6;
		while(capacity < min_value) capacity <<= 1;
		if(addr == nullptr)
			addr = new char_type[capacity];
		else
		{
			char_type *target_addr = new char_type[capacity];
			copy_memory(addr, target_addr, size * sizeof(char_type));
			delete[] addr;
			addr = target_addr;
		}
	}

	/*idx must be valid array position*/
	void insert(uint64 idx, const char_type &value)
	{
		if(size == capacity) increase_capacity(1);
		move_memory(addr + idx, addr + idx + 1, (size - idx) * sizeof(char_type));
		size++;
		addr[idx] = value;
	}

	/*idx must be valid array position*/
	void insert_range(uint64 idx, const char_type *begin, const char_type *end)
	{
		uint64 count = end - begin;
		if(capacity < size + count)
			increase_capacity(size + count - capacity);
		move_memory(addr + idx, addr + idx + count, (size - idx) * sizeof(char_type));
		size += count;
		copy_memory(begin, addr + idx, count * sizeof(char_type));
	}

	/*Inserts element and calls default constructor
	idx must be valid array position*/
	void insert_default(uint64 idx, uint64 count)
	{
		if(capacity < size + count)
			increase_capacity(size + count - capacity);
		move_memory(addr + idx, addr + idx + count, (size - idx) * sizeof(char_type));
		construct_range(addr + idx, addr + idx + count);
		size += count;
	}

	/*Inserts elements to the end of array*/
	void push(const char_type &value)
	{
		if(size == capacity) increase_capacity(1);
		addr[size] = value;
		size++;
	}

	/*Inserts element to the end of array and calls default constructor*/
	void push_default()
	{
		if(size == capacity) increase_capacity(1);
		size++;
		construct(addr + size - 1);
	}

	/*idx must be valid array position*/
	void remove(uint64 idx)
	{
		move_memory(addr + idx + 1, addr + idx, (size - idx - 1) * sizeof(char_type));
		size--;
	}

	/*Removes elements in [begin, end) range
	begin, end must be valid array position values*/
	void remove_range(uint64 begin, uint64 end)
	{
		move_memory(addr + end, addr + begin, (size - end) * sizeof(char_type));
		size -= end - begin;
	}

	/*Removes element from the end of array
	array must have at least 1 element*/
	void pop()
	{
		size--;
	}

	/*Removes all elements from array, but doesnt affect capacity*/
	void clear()
	{
		size = 0;
	}

	/*Removes all elements from array and resets capacity to 0*/
	void reset()
	{
		if(addr == nullptr) return;
		delete[] addr;
		addr = nullptr;
		size = 0;
		capacity = 0;
	}
	/*Get reference to first element of array
	array must have at least 1 element*/
	char_type &front()
	{
		return addr[0];
	}

	/*Get reference to last element of array
	array must have at least 1 element*/
	char_type &back()
	{
		return addr[size - 1];
	}

	/*Get reference to element of array by idx
	idx must be valid array position*/
	char_type &operator[](uint64 idx)
	{
		return addr[idx];
	}

	string &operator<<(string &source)
	{
		insert_range(size, source.addr, source.addr + source.size);
		return *this;
	}

	string &operator<<(char_type ch)
	{
		push(ch);
		return *this;
	}

	string &operator<<(const char_type *source)
	{
		uint64 length = 0;
		while(source[length] != 0) length++;
		insert_range(size, source, source + length);
		return *this;
	}
	
	string &operator<<(int8 value)
	{
		return *this << int64(value);
	}

	string &operator<<(uint8 value)
	{
		return *this << uint64(value);
	}

	string &operator<<(int16 value)
	{
		return *this << int64(value);
	}

	string &operator<<(uint16 value)
	{
		return *this << uint64(value);
	}

	string &operator<<(int32 value)
	{
		return *this << int64(value);
	}

	string &operator<<(uint32 value)
	{
		return *this << uint64(value);
	}

	string &operator<<(int64 value)
	{
		if(value < 0)
		{
			push(char_type('-'));
			value = -value;
		}
		return *this << uint64(value);
	}
	
	string &operator<<(uint64 value)
	{
		if(value == 0) push(char_type('0'));
		uint64 position = size;
		while(value != 0)
		{
			insert(position, char_type(value % 10 + uint64('0')));
			value /= 10;
		}
		return *this;
	}
	
	string &operator<<(real value)
	{
		if(value.negative) push(U'-');
		*this << uint64(value.integer);
		uint64 position = size;
		push(char_type('.'));
		*this << uint64(value.fraction);
		while(size - position - 1 < string_config()->min_fraction_digits)
			push(char_type('0'));
		while(size - position - 1 > string_config()->max_fraction_digits)
			pop();
		if(position == size - 1) pop();
		return *this;
	}
	
	string &operator<<=(string &source)
	{
		clear();
		return *this << source;
	}
	
	string &operator<<=(char_type ch)
	{
		clear();
		return *this << ch;
	}
	
	string &operator<<=(const char_type *source)
	{
		clear();
		return *this << source;
	}

	string &operator<<=(int8 value)
	{
		clear();
		return *this << value;
	}
	
	string &operator<<=(uint8 value)
	{
		clear();
		return *this << value;
	}
	
	string &operator<<=(int16 value)
	{
		clear();
		return *this << value;
	}

	string &operator<<=(uint16 value)
	{
		clear();
		return *this << value;
	}

	string &operator<<=(int32 value)
	{
		clear();
		return *this << value;
	}

	string &operator<<=(uint32 value)
	{
		clear();
		return *this << value;
	}

	string &operator<<=(int64 value)
	{
		clear();
		return *this << value;
	}
	
	string &operator<<=(uint64 value)
	{
		clear();
		return *this << value;
	}
	
	string &operator<<=(real value)
	{
		clear();
		return *this << value;
	}

	bool operator==(const string &str) const
	{
		if(size != str.size) return false;
		for(uint64 i = 0; i < size; i++)
			if(addr[i] != str.addr[i]) return false;
		return true;
	}

	bool operator!=(const string &str) const
	{
		return !(*this == str);
	}

	bool operator<(const string &str) const
	{
		uint64 len = min(size, str.size);
		for(uint64 i = 0; i < len; i++)
			if(addr[i] < str.addr[i]) return true;
			else if(addr[i] > str.addr[i]) return false;
		return size < str.size;
	}

	bool operator>(const string &str) const
	{
		uint64 len = min(size, str.size);
		for(uint64 i = 0; i < len; i++)
			if(addr[i] > str.addr[i]) return true;
			else if(addr[i] < str.addr[i]) return false;
		return size > str.size;
	}
};

typedef string<char8> u8string;
typedef string<char16> u16string;
typedef string<char32> u32string;
typedef string<wchar> wstring;

template<typename char_type> struct copier<string<char_type>>
{
	void operator()(string<char_type> &input, string<char_type> *output)
	{
		output->clear();
		output->insert_range(0, input.addr, input.addr + input.size);
	}
};

template<typename char_type> struct string_stream
{
	/*addr must be valid pointer set before each operation*/
	char_type *addr;
	/*addr must be pointer with at least size elements of char_type*/
	uint64 size;
	/*Current array position set before each operation
	default value - 0
	position must be valid index*/
	uint64 position;
	/*char_skiped is set after each convert operation*/
	uint64 chars_skiped;
	/*chars_converted is set after each convert operation*/
	uint64 chars_converted;

	string_stream()
	{
		position = 0;
	}

	string_stream(const char_type *string_addr, uint64 string_size)
	{
		addr = const_cast<char_type *>(string_addr);
		size = string_size;
		position = 0;
	}

	/*Signals that stream is ended and position value equals to size value*/
	bool ended()
	{
		return position == size;
	}

	void operator>>(char_type &ch)
	{
		if(ended())
		{
			chars_skiped = 0;
			chars_converted = 0;
			return;
		}
		ch = addr[position];
		position++;
		chars_skiped = 0;
		chars_converted = 1;
	}

	void operator>>(int8 &value)
	{
		int64 result;
		*this >> result;
		value = int8(result);
	}

	void operator>>(uint8 &value)
	{
		uint64 result;
		*this >> result;
		value = uint8(result);
	}

	void operator>>(int16 &value)
	{
		int64 result;
		*this >> result;
		value = int16(result);
	}

	void operator>>(uint16 &value)
	{
		uint64 result;
		*this >> result;
		value = uint16(result);
	}

	void operator>>(int32 &value)
	{
		int64 result;
		*this >> result;
		value = int32(result);
	}

	void operator>>(uint32 &value)
	{
		uint64 result;
		*this >> result;
		value = uint32(result);
	}

	void operator>>(int64 &value)
	{
		int64 result = 0;
		chars_skiped = 0;
		chars_converted = 0;
		while(!ended() && (addr[position] < char_type('0') || addr[position] > char_type('9')))
		{
			position++;
			chars_skiped++;
		}
		uint64 num_iter = position;
		while(num_iter != size && addr[num_iter] >= char_type('0') && addr[num_iter] <= char_type('9'))
		{
			result *= 10;
			result += int64(addr[num_iter]) - int64('0');
			chars_converted++;
			num_iter++;
		}
		if(chars_converted != 0)
		{
			if(chars_skiped != 0 && addr[position - 1] == char_type('-'))
				result = -result;
			position = num_iter;
			value = result;
		}
	}

	void operator>>(uint64 &value)
	{
		uint64 result = 0;
		chars_skiped = 0;
		chars_converted = 0;
		while(!ended() && (addr[position] < char_type('0') || addr[position] > char_type('9')))
		{
			position++;
			chars_skiped++;
		}
		uint64 num_iter = position;
		while(num_iter != size && addr[num_iter] >= char_type('0') && addr[num_iter] <= char_type('9'))
		{
			result *= 10;
			result += uint64(addr[num_iter]) - uint64('0');
			chars_converted++;
			num_iter++;
		}
		if(chars_converted != 0)
		{
			position = num_iter;
			value = result;
		}
	}
	
	void operator>>(real &value)
	{
		real result;
		int64 integer;
		*this >> integer;
		if(chars_converted != 0)
		{
			if(integer < 0)
			{
				result.negative = true;
				result.integer = uint32(-integer);
			}
			else
			{
				result.negative = false;
				result.integer = uint32(integer);
			}
			if(!ended() && addr[position] == '.'
				&& position + 1 != size
				&& addr[position + 1] >= char_type('0') && addr[position + 1] <= char_type('9'))
			{
				position++;
				chars_converted++;
				uint32 chars_converted_temp = chars_converted,
					chars_skiped_temp = chars_skiped;
				uint64 fraction;
				*this >> fraction;
				result.fraction = uint32(fraction);
				chars_skiped = chars_skiped_temp;
				chars_converted += chars_converted_temp;
			}
			else result.fraction = 0;
			value = result;
		}
	}
};
