#pragma once
#include "global_operators.h"

template<typename value_type, typename comparator = cmp_less<value_type>> class sorted_array
{
protected:
	value_type *addr;
	uint64 length;
	uint64 max_length;

	void index_insert(uint64 idx, const value_type &value)
	{
		if(length == max_length) increase_capacity(1);
		move_memory(addr + idx, addr + idx + 1, (length - idx) * sizeof(value_type));
		construct(addr + idx);
		addr[idx] = value;
		length++;
	}
public:
	sorted_array()
	{
		addr = nullptr;
		length = 0;
		max_length = 0;
	}

	sorted_array(sorted_array &arr)
	{
		addr = nullptr;
		length = 0;
		max_length = 0;
		*this = arr;
	}

	~sorted_array()
	{
		if(addr == nullptr) return;
		destroy_range(addr, addr + length);
		delete[] (byte *)addr;
	}

	void increase_capacity(uint64 min_value)
	{
		min_value += max_length;
		if(max_length == 0) max_length = 6;
		while(max_length < min_value) max_length <<= 1;
		if(addr == nullptr)
			addr = (value_type *)(new byte[max_length * sizeof(value_type)]);
		else
		{
			value_type *target_addr = (value_type *)(new byte[max_length * sizeof(value_type)]);
			copy_memory(addr, target_addr, length * sizeof(value_type));
			delete[] (byte *)(addr);
			addr = target_addr;
		}
	}
	
	uint64 lower_bound(const value_type &value)
	{
		if(length == 0) return 0;
		if(comparator()(addr[length - 1], value)) return length;
		uint64 l = 0, r = length - 1, m;
		while(true)
		{
			m = (l + r) / 2;
			if(l == m)
			{
				if(!comparator()(addr[l], value))
					return l;
				else return r;
			}
			if(!comparator()(addr[m], value))
				r = m;
			else l = m;
		}
	}

	uint64 upper_bound(const value_type &value)
	{
		if(length == 0) return 0;
		if(comparator()(value, addr[0])) return length;
		uint64 l = 0, r = length - 1, m;
		while(true)
		{
			m = (l + r) / 2;
			if(l == m)
			{
				if(!comparator()(value, addr[r]))
					return r;
				else return l;
			}
			if(!comparator()(value, addr[m]))
				l = m;
			else r = m;
		}
	}

	uint64 find(const value_type &value)
	{
		uint64 idx = lower_bound(value);
		if(idx == length || comparator()(value, addr[idx]))
			return length;
		else return idx;
	}
	
	void insert(const value_type &value)
	{
		uint64 idx = upper_bound(value);
		if(idx == length) index_insert(0, value);
		else index_insert(idx + 1, value);
	}

	void insert_default(uint64 count)
	{
		value_type value;
		while(count != 0)
		{
			insert(value);
			count--;
		}
	}

	void insert_range(const value_type *begin, const value_type *end)
	{
		uint64 count = end - begin;
		if(max_length < length + count)
			increase_capacity(length + count - max_length);
		length += count;
		while(begin != end)
		{
			insert(*begin);
			begin++;
		}
	}

	void remove(const value_type &value)
	{
		uint64 l = lower_bound(value), r = l;
		while(r != length && !comparator()(value, addr[r])) r++;
		remove_range(l, r);
	}

	void remove_at(uint64 idx)
	{
		destroy(addr + idx);
		move_memory(addr + idx + 1, addr + idx, (length - idx - 1) * sizeof(value_type));
		length--;
	}

	void remove_range(uint64 begin, uint64 end)
	{
		destroy_range(addr + begin, addr + end);
		move_memory(addr + end, addr + begin, (length - end) * sizeof(value_type));
		length -= end - begin;
	}

	void pop()
	{
		destroy(addr + length - 1);
		length--;
	}

	void clear()
	{
		destroy_range(addr, addr + length);
		length = 0;
	}

	void reset()
	{
		if(addr == nullptr) return;
		destroy_range(addr, addr + length);
		delete[] (byte *)(addr);
		addr = nullptr;
		length = 0;
		max_length = 0;
	}

	value_type &at(uint64 idx)
	{
		return addr[idx];
	}

	value_type &front()
	{
		return addr[0];
	}

	value_type &back()
	{
		return addr[length - 1];
	}

	uint64 size()
	{
		return length;
	}

	uint64 capacity()
	{
		return max_length;
	}

	value_type &operator[](uint64 idx)
	{
		return addr[idx];
	}

	sorted_array &operator=(sorted_array &arr)
	{
		clear();
		increase_capacity(arr.length);
		length = arr.length;
		for(uint64 i = 0; i < length; i++)
			addr[i] = arr.addr[i];
		return *this;
	}
};
