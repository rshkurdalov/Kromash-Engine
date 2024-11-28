#pragma once
#include "global_operators.h"

template<typename value_type> struct array
{
	/*If array is empty <addr> is nullptr*/
	value_type *addr;
	/*If <addr> is nullptr - size must be 0*/
	uint64 size;
	/*If <addr> is nullptr - capacity must be 0*/
	uint64 capacity;

	array()
	{
		addr = nullptr;
		size = 0;
		capacity = 0;
	}

	~array()
	{
		if(addr == nullptr) return;
		destroy_range(addr, addr + size);
		delete[] (byte *)addr;
	}

	/*Increase capacity by a minimum of <min_value> elements*/
	void increase_capacity(uint64 min_value)
	{
		min_value += capacity;
		if(capacity == 0) capacity = 6;
		while(capacity < min_value) capacity <<= 1;
		if(addr == nullptr)
			addr = (value_type *)(new byte[capacity * sizeof(value_type)]);
		else
		{
			value_type *target_addr = (value_type *)(new byte[capacity * sizeof(value_type)]);
			copy_memory(addr, target_addr, size * sizeof(value_type));
			delete[] (byte *)(addr);
			addr = target_addr;
		}
	}

	/*Ensure capacity by a minimum of <min_value> elements*/
	void ensure_capacity(uint64 min_value)
	{
		if(capacity < min_value) increase_capacity(min_value - capacity);
	}

	/*<idx> must be valid array position*/
	void insert(uint64 idx, const value_type &value)
	{
		if(size == capacity) increase_capacity(1);
		move_memory(addr + idx, addr + idx + 1, (size - idx) * sizeof(value_type));
		construct(addr + idx);
		copy(value, addr + idx);
		size++;
	}

	/*<idx> must be valid array position*/
	void insert_moving(uint64 idx, value_type &&value)
	{
		if(size == capacity) increase_capacity(1);
		move_memory(addr + idx, addr + idx + 1, (size - idx) * sizeof(value_type));
		construct(addr + idx);
		move(value, addr + idx);
		size++;
	}

	/*<idx> must be valid array position*/
	void insert_range(uint64 idx, const value_type *begin, const value_type *end)
	{
		uint64 count = end - begin;
		if(capacity < size + count)
			increase_capacity(size + count - capacity);
		move_memory(addr + idx, addr + idx + count, (size - idx) * sizeof(value_type));
		size += count;
		while(begin != end)
		{
			construct(addr + idx);
			copy(*begin, addr + idx);
			begin++;
			idx++;
		}
	}

	/*Insert element and call default constructor
	<idx> must be valid array position*/
	void insert_default(uint64 idx, uint64 count)
	{
		if(capacity < size + count)
			increase_capacity(size + count - capacity);
		move_memory(addr + idx, addr + idx + count, (size - idx) * sizeof(value_type));
		construct_range(addr + idx, addr + idx + count);
		size += count;
	}

	/*Insert element to the end of array*/
	void push(const value_type &value)
	{
		if(size == capacity) increase_capacity(1);
		construct(addr + size);
		copy(value, addr + size);
		size++;
	}

	/*Insert element to the end of array*/
	void push_moving(value_type &&value)
	{
		if(size == capacity) increase_capacity(1);
		construct(addr + size);
		move(value, addr + size);
		size++;
	}

	/*Insert element to the end of array and call default constructor*/
	void push_default()
	{
		if(size == capacity) increase_capacity(1);
		size++;
		construct(addr + size - 1);
	}

	/*<idx> must be valid array position*/
	void remove(uint64 idx)
	{
		destroy(addr + idx);
		move_memory(addr + idx + 1, addr + idx, (size - idx - 1) * sizeof(value_type));
		size--;
	}

	/*Remove elements in [begin, end) range
	<begin>, <end> must be valid array position values*/
	void remove_range(uint64 begin, uint64 end)
	{
		destroy_range(addr + begin, addr + end);
		move_memory(addr + end, addr + begin, (size - end) * sizeof(value_type));
		size -= end - begin;
	}

	/*Remove element from the end of array
	array must have at least 1 element*/
	void pop()
	{
		destroy(addr + size - 1);
		size--;
	}

	/*Remove scecific element from array*/
	void remove_element(const value_type &value)
	{
		for(uint64 i = 0; i < size; i++)
			if(addr[i] == value) remove(i--);
	}

	/*Removes all elements from array, but doesnt affect capacity*/
	void clear()
	{
		destroy_range(addr, addr + size);
		size = 0;
	}

	/*Removes all elements from array and resets capacity to 0*/
	void reset()
	{
		if(addr == nullptr) return;
		destroy_range(addr, addr + size);
		delete[] (byte *)(addr);
		addr = nullptr;
		size = 0;
		capacity = 0;
	}

	/*Get reference to first element of array
	array must have at least 1 element*/
	value_type &front()
	{
		return addr[0];
	}

	/*Get reference to last element of array
	array must have at least 1 element*/
	value_type &back()
	{
		return addr[size - 1];
	}

	/*Get reference to element of array by <idx>
	<idx> must be valid array position*/
	value_type &operator[](uint64 idx)
	{
		return addr[idx];
	}

	/*Get position of lowest value that is not less than key_value
	if such values are several - one with lowest position returns
	array must be sorted*/
	uint64 lower_bound(const key<value_type> &key_value)
	{
		if(size == 0) return 0;
		if(key<value_type>(addr[size - 1]) < key_value) return size;
		uint64 l = 0, r = size - 1, m;
		while(true)
		{
			m = (l + r) / 2;
			if(l == m)
			{
				if(!(key<value_type>(addr[l]) < key_value))
					return l;
				else return r;
			}
			if(!(key<value_type>(addr[m]) < key_value))
				r = m;
			else l = m;
		}
	}

	/*Get position of highest value that is not greater than key_value
	if such values are several - one with highest position returns
	array must be sorted*/
	uint64 upper_bound(const key<value_type> &key_value)
	{
		if(size == 0) return 0;
		if(key_value < key<value_type>(addr[0])) return size;
		uint64 l = 0, r = size - 1, m;
		while(true)
		{
			m = (l + r) / 2;
			if(l == m)
			{
				if(!(key_value < key<value_type>(addr[r])))
					return r;
				else return l;
			}
			if(!(key_value < key<value_type>(addr[m])))
				l = m;
			else r = m;
		}
	}

	/*Find position of key_value
	array must be sorted*/
	uint64 binary_search(const key<value_type> &key_value)
	{
		uint64 idx = lower_bound(key_value);
		if(idx == size || key_value < key<value_type>(addr[idx]))
			return size;
		else return idx;
	}
	
	/*Insert value to the sorted array
	array must be sorted*/
	void binary_insert(const value_type &value)
	{
		uint64 idx = upper_bound(key<value_type>(value));
		if(idx == size) insert(0, value);
		else insert(idx + 1, value);
	}

	/*Remove value from sorted array
	array must be sorted*/
	void binary_remove(const key<value_type> &key_value)
	{
		uint64 l = lower_bound(key_value), r = l;
		while(r != size && !(key_value < key<value_type>(addr[r]))) r++;
		remove_range(l, r);
	}
};

template<typename value_type> struct copier<array<value_type>>
{
	void operator()(const array<value_type> &input, array<value_type> *output)
	{
		output->clear();
		output->insert_range(0, input.addr, input.addr + input.size);
	}
};

template<typename value_type> struct mover<array<value_type>>
{
	void operator()(array<value_type> &input, array<value_type> *output)
	{
		output->addr = input.addr;
		output->size = input.size;
		output->capacity = input.capacity;
		input.addr = nullptr;
		input.size = 0;
		input.capacity = 0;
	}
};
