#pragma once
#include "file.h"

template<typename value_type> struct fileset_node
{
	value_type value;
	int64 upper;
	int64 left;
	int64 right;
	int32 diff;
};

template<typename value_type> struct fileset_iterator
{
	int64 idx;
	fileset_node<value_type> node;
};

template<typename value_type> struct fileset
{
	file storage;
	int64 root_idx;
	int64 free_idx;
	uint64 size;

	void open()
	{
		if(!storage.exists())
		{
			storage.open();
			clear();
		}
		else storage.open();
		storage.read(8, &root_idx);
		storage.read(8, &free_idx);
		storage.read(8, &size);
	}

	void close()
	{
		storage.close();
	}

	fileset_iterator<value_type> iterator(int64 idx)
	{
		fileset_iterator<value_type> iter;
		iter.idx = idx;
		if(iter.idx == -1) return iter;
		storage.position = uint64(24 + idx * sizeof(fileset_node<value_type>));
		storage.read(sizeof(fileset_node<value_type>), &iter.node);
		return iter;
	}

	void save_node(fileset_iterator<value_type> iter)
	{
		if(iter.idx == -1) return;
		storage.position = uint64(24 + iter.idx * sizeof(fileset_node<value_type>));
		storage.write(&iter.node, sizeof(fileset_node<value_type>));
	}

	fileset_iterator<value_type> begin()
	{
		fileset_iterator<value_type> iter;
		if(root_idx == -1)
		{
			iter.idx = -1;
			return iter;
		}
		iter = iterator(root_idx);
		while(iter.node.left != -1)
			iter = iterator(iter.node.left);
		return iter;
	}

	fileset_iterator<value_type> end()
	{
		fileset_iterator<value_type> iter;
		if(root_idx == -1)
		{
			iter.idx = -1;
			return iter;
		}
		iter = iterator(root_idx);
		while(iter.node.right != -1)
			iter = iterator(iter.node.right);
		return iter;
	}

	fileset_iterator<value_type> next(fileset_iterator<value_type> iter)
	{
		if(iter.node.right != -1)
		{
			iter = iterator(iter.node.right);
			while(iter.node.left != -1)
				iter = iterator(iter.node.left);
			return iter;
		}
		else
		{
			while(iter.node.upper != -1 && iterator(iter.node.upper).node.right == iter.idx)
				iter = iterator(iter.node.upper);
			return iterator(iter.node.upper);
		}
	}

	fileset_iterator<value_type> prev(fileset_iterator<value_type> iter)
	{
		if(iter.node.left != -1)
		{
			iter = iterator(iter.node.left);
			while(iter.node.right != -1)
				iter = iterator(iter.node.right);
			return iter;
		}
		else
		{
			while(iter.node.upper != -1 && iterator(iter.node.upper).node.left == iter.idx)
				iter = iterator(iter.node.upper);
			return iterator(iter.node.upper);
		}
	}

	fileset_iterator<value_type> lower_bound(const key<value_type> &key_value)
	{
		fileset_iterator<value_type> iter;
		if(root_idx == -1)
		{
			iter.idx = -1;
			return iter;
		}
		iter = iterator(root_idx);
		while(true)
		{
			if(!(key<value_type>(iter.node.value) < key_value))
			{
				if(iter.node.left != -1)
					iter = iterator(iter.node.left);
				else break;
			}
			else
			{
				if(iter.node.right != -1)
					iter = iterator(iter.node.right);
				else break;
			}
		}
		while(iter.idx != -1 && key<value_type>(iter.node.value) < key_value)
			iter = next(iter);
		return iter;
	}

	fileset_iterator<value_type> upper_bound(const key<value_type> &key_value)
	{
		fileset_iterator<value_type> iter;
		if(root_idx == -1)
		{
			iter.idx = -1;
			return iter;
		}
		iter = iterator(root_idx);
		while(true)
		{
			if(!(key_value < key<value_type>(iter.node.value)))
			{
				if(iter.node.right != -1)
					iter = iterator(iter.node.right);
				else break;
			}
			else
			{
				if(iter.node.left != -1)
					iter = iterator(iter.node.left);
				else break;
			}
		}
		while(iter.idx != -1 && key_value < key<value_type>(iter.node.value))
			iter = prev(iter);
		return iter;
	}

	fileset_iterator<value_type> find(const key<value_type> &key_value)
	{
		fileset_iterator<value_type> iter = lower_bound(key_value);
		if(iter.idx == -1 || key_value < key<value_type>(iter.node.value))
			iter.idx = -1;
		return iter;
	}

	void rotate_left(fileset_iterator<value_type> &iter)
	{
		fileset_iterator<value_type> t = iterator(iter.node.right);
		iter.node.right = t.node.left;
		fileset_iterator<value_type> r = iterator(iter.node.right);
		if(r.idx != -1)
			r.node.upper = iter.idx;
		t.node.left = iter.idx;
		t.node.upper = iter.node.upper;
		iter.node.upper = t.idx;
		if(t.node.upper == -1)
		{
			root_idx = t.idx;
			storage.position = 0;
			storage.write(&root_idx, 8);
		}
		else
		{
			fileset_iterator<value_type> u = iterator(t.node.upper);
			if(u.node.left == iter.idx)
				u.node.left = t.idx;
			else u.node.right = t.idx;
			save_node(u);
		}
		save_node(t);
		save_node(iter);
		save_node(r);
	}

	void rotate_right(fileset_iterator<value_type> &iter)
	{
		fileset_iterator<value_type> t = iterator(iter.node.left);
		iter.node.left = t.node.right;
		fileset_iterator<value_type> l = iterator(iter.node.left);
		if(l.idx != -1)
			l.node.upper = iter.idx;
		t.node.right = iter.idx;
		t.node.upper = iter.node.upper;
		iter.node.upper = t.idx;
		if(t.node.upper == -1)
		{
			root_idx = t.idx;
			storage.position = 0;
			storage.write(&root_idx, 8);
		}
		else
		{
			fileset_iterator<value_type> u = iterator(t.node.upper);
			if(u.node.left == iter.idx)
				u.node.left = t.idx;
			else u.node.right = t.idx;
			save_node(u);
		}
		save_node(t);
		save_node(iter);
		save_node(l);
	}

	void rebalance(fileset_iterator<value_type> &iter)
	{
		if(iter.node.diff == -2)
		{
			fileset_iterator<value_type> r = iterator(iter.node.right);
			if(r.node.diff != 1)
			{
				if(r.node.diff == -1)
				{
					iter.node.diff = 0;
					r.node.diff = 0;
				}
				else
				{
					iter.node.diff = -1;
					r.node.diff = 1;
				}
				save_node(iter);
				save_node(r);
				rotate_left(iter);
			}
			else
			{
				fileset_iterator<value_type> rl = iterator(r.node.left);
				if(rl.node.diff == -1)
				{
					iter.node.diff = 1;
					r.node.diff = 0;
				}
				else if(rl.node.diff == 0)
				{
					iter.node.diff = 0;
					r.node.diff = 0;
				}
				else
				{
					iter.node.diff = 0;
					r.node.diff = -1;
				}
				rl.node.diff = 0;
				save_node(iter);
				save_node(r);
				save_node(rl);
				rotate_right(r);
				iter = iterator(iter.idx);
				rotate_left(iter);
			}
		}
		else
		{
			fileset_iterator<value_type> l = iterator(iter.node.left);
			if(l.node.diff != -1)
			{
				if(l.node.diff == 1)
				{
					iter.node.diff = 0;
					l.node.diff = 0;
				}
				else
				{
					iter.node.diff = 1;
					l.node.diff = -1;
				}
				save_node(iter);
				save_node(l);
				rotate_right(iter);
			}
			else
			{
				fileset_iterator<value_type> lr = iterator(l.node.right);
				if(lr.node.diff == 1)
				{
					iter.node.diff = -1;
					l.node.diff = 0;
				}
				else if(lr.node.diff == 0)
				{
					iter.node.diff = 0;
					l.node.diff = 0;
				}
				else
				{
					iter.node.diff = 0;
					l.node.diff = 1;
				}
				lr.node.diff = 0;
				save_node(iter);
				save_node(l);
				save_node(lr);
				rotate_left(l);
				iter = iterator(iter.idx);
				rotate_right(iter);
			}
		}
	}

	void insert(const value_type &value)
	{
		fileset_iterator<value_type> node, prev_node, new_node;
		if(free_idx == -1)
		{
			new_node.idx = (storage.size - 24) / sizeof(fileset_node<value_type>);
			storage.resize(storage.size + sizeof(fileset_node<value_type>));
		}
		else
		{
			new_node.idx = free_idx;
			node = iterator(free_idx);
			free_idx = node.node.left;
			storage.position = 8;
			storage.write(&free_idx, 8);
		}
		new_node.node.value = value;
		new_node.node.left = -1;
		new_node.node.right = -1;
		new_node.node.diff = 0;
		size++;
		storage.position = 16;
		storage.write(&size, 8);
		if(root_idx == -1)
		{
			root_idx = new_node.idx;
			storage.position = 0;
			storage.write(&root_idx, 8);
			new_node.node.upper = -1;
		}
		else
		{
			key<value_type> key_value(value);
			node = iterator(root_idx);
			while(true)
			{
				if(key_value < key<value_type>(node.node.value))
				{
					if(node.node.left != -1)
						node = iterator(node.node.left);
					else
					{
						node.node.left = new_node.idx;
						save_node(node);
						break;
					}
				}
				else
				{
					if(node.node.right != -1)
						node = iterator(node.node.right);
					else
					{
						node.node.right = new_node.idx;
						save_node(node);
						break;
					}
				}
			}
			new_node.node.upper = node.idx;
		}
		save_node(new_node);
		node = new_node;
		while(true)
		{
			prev_node = node;
			node = iterator(node.node.upper);
			if(node.idx == -1) break;
			if(node.node.left == prev_node.idx)
				node.node.diff++;
			else node.node.diff--;
			save_node(node);
			if(node.node.diff == 0) break;
			else if(node.node.diff == 2 || node.node.diff == -2)
			{
				rebalance(node);
				node = iterator(node.node.upper);
				if(node.node.diff == 0) break;
			}
		}
	}

	void remove(fileset_iterator<value_type> *iter)
	{
		fileset_iterator<value_type> swap_node, prev_node, next_iter;
		size--;
		storage.position = 16;
		storage.write(&size, 8);
		next_iter = next(*iter);
		if(iter->node.left != -1 || iter->node.right != -1)
		{
			swap_node = prev(*iter);
			if(swap_node.idx == -1)
			{
				fileset_iterator<value_type> r = iterator(iter->node.right);
				static_copy<sizeof(value_type)>(&r.node.value, &iter->node.value);
				save_node(*iter);
				*iter = r;
			}
			else
			{
				static_copy<sizeof(value_type)>(&swap_node.node.value, &iter->node.value);
				save_node(*iter);
				if(swap_node.node.left != -1)
				{
					fileset_iterator<value_type> l = iterator(swap_node.node.left);
					static_copy<sizeof(value_type)>(&l.node.value, &swap_node.node.value);
					save_node(swap_node);
					*iter = l;
				}
				else *iter = swap_node;
			}
		}
		if(iter->node.upper == -1)
		{
			clear();
			next_iter.idx = -1;
			return;
		}
		else
		{
			fileset_iterator<value_type> u = iterator(iter->node.upper);
			if(u.node.right == iter->idx)
			{
				u.node.right = -1;
				u.node.diff++;
			}
			else
			{
				u.node.left = -1;
				u.node.diff--;
			}
			save_node(u);
			swap_node = u;
			iter->node.left = free_idx;
			save_node(*iter);
			free_idx = iter->idx;
			storage.position = 8;
			storage.write(&free_idx, 8);
			*iter = swap_node;
		}
		while(true)
		{
			
			if(iter->node.diff == 1 || iter->node.diff == -1) break;
			else if(iter->node.diff == 2 || iter->node.diff == -2)
			{
				rebalance(*iter);
				*iter = iterator(iter->node.upper);
				if(iter->node.diff != 0) break;
			}
			prev_node = *iter;
			*iter = iterator(iter->node.upper);
			if(iter->idx == -1) break;
			if(iter->node.left == prev_node.idx)
				iter->node.diff--;
			else iter->node.diff++;
			save_node(*iter);
		}
		*iter = iterator(next_iter.idx);
	}

	void remove_value(const key<value_type> &key_value)
	{
		fileset_iterator<value_type> node;
		while(true)
		{
			node = find(key_value);
			if(node.idx == -1) break;
			remove(&node);
		}
	}

	void clear()
	{
		storage.resize(24);
		storage.position = 0;
		root_idx = -1;
		storage.write(&root_idx, 8);
		free_idx = -1;
		storage.write(&free_idx, 8);
		size = 0;
		storage.write(&size, 8);
	}
};

#pragma pack(push, 1)
struct parametric_fileset_node
{
	void *value;
	int64 upper;
	int64 left;
	int64 right;
	int32 diff;

	parametric_fileset_node()
	{
		value = nullptr;
	}

	parametric_fileset_node(uint64 element_size)
	{
		value = new byte[element_size];
	}

	~parametric_fileset_node()
	{
		if(value != nullptr) delete[] (byte *)(value);
	}

	void initialize(uint64 element_size)
	{
		value = new byte[element_size];
	}
};
#pragma pack(pop)

struct parametric_fileset_iterator
{
	int64 idx;
	parametric_fileset_node node;

	parametric_fileset_iterator() {}

	parametric_fileset_iterator(uint64 element_size) : node(element_size) {}
};

struct parametric_fileset
{
	file storage;
	uint64 element_size;
	int64 root_idx;
	int64 free_idx;
	uint64 size;

	void open()
	{
		if(!storage.exists())
		{
			storage.open();
			clear();
		}
		else storage.open();
		storage.read(8, &root_idx);
		storage.read(8, &free_idx);
		storage.read(8, &size);
		storage.read(8, &element_size);
	}

	void close()
	{
		storage.close();
	}

	void iterator(int64 idx, parametric_fileset_iterator *iter)
	{
		iter->idx = idx;
		if(iter->idx == -1) return;
		storage.position = uint64(32 + idx * (sizeof(parametric_fileset_node) - sizeof(void *) + element_size));
		storage.read(element_size, iter->node.value);
		storage.read(sizeof(parametric_fileset_node) - sizeof(void *), &iter->node.upper);
	}

	void iterator_no_data(int64 idx, parametric_fileset_iterator *iter)
	{
		iter->idx = idx;
		if(iter->idx == -1) return;
		storage.position = uint64(32 + idx * (sizeof(parametric_fileset_node) - sizeof(void *) + element_size)) + element_size;
		storage.read(sizeof(parametric_fileset_node) - sizeof(void *), &iter->node.upper);
	}

	void save_node(parametric_fileset_iterator &iter)
	{
		if(iter.idx == -1) return;
		storage.position = uint64(32 + iter.idx * (sizeof(parametric_fileset_node) - sizeof(void *) + element_size));
		storage.write(iter.node.value, element_size);
		storage.write(&iter.node.upper, sizeof(parametric_fileset_node) - sizeof(void *));
	}

	void begin(parametric_fileset_iterator *iter)
	{
		if(root_idx == -1)
		{
			iter->idx = -1;
			return;
		}
		iterator_no_data(root_idx, iter);
		while(iter->node.left != -1)
			iterator_no_data(iter->node.left, iter);
		iterator(iter->idx, iter);
	}

	void end(parametric_fileset_iterator *iter)
	{
		if(root_idx == -1)
		{
			iter->idx = -1;
			return;
		}
		iterator_no_data(root_idx, iter);
		while(iter->node.right != -1)
			iterator(iter->node.right, iter);
		iterator(iter->idx, iter);
	}

	void next(parametric_fileset_iterator *iter)
	{
		if(iter->node.right != -1)
		{
			iterator_no_data(iter->node.right, iter);
			while(iter->node.left != -1)
				iterator_no_data(iter->node.left, iter);
		}
		else
		{
			uint64 prev_idx;
			do
			{
				prev_idx = iter->idx;
				iterator_no_data(iter->node.upper, iter);
			}
			while(iter->idx != -1 && iter->node.right == prev_idx);
		}
		iterator(iter->idx, iter);
	}

	void prev(parametric_fileset_iterator *iter)
	{
		if(iter->node.left != -1)
		{
			iterator_no_data(iter->node.left, iter);
			while(iter->node.right != -1)
				iterator_no_data(iter->node.right, iter);
		}
		else
		{
			uint64 prev_idx;
			do
			{
				prev_idx = iter->idx;
				iterator_no_data(iter->node.upper, iter);
			}
			while(iter->idx != -1 && iter->node.left == prev_idx);
		}
		iterator(iter->idx, iter);
	}

	template<typename comparator>
	void lower_bound(void *value, comparator cmp, parametric_fileset_iterator *iter)
	{
		if(root_idx == -1)
		{
			iter->idx = -1;
			return;
		}
		iterator(root_idx, iter);
		while(true)
		{
			if(!cmp(iter->node.value, value))
			{
				if(iter->node.left != -1)
					iterator(iter->node.left, iter);
				else break;
			}
			else
			{
				if(iter->node.right != -1)
					iterator(iter->node.right, iter);
				else break;
			}
		}
		while(iter->idx != -1 && cmp(iter->node.value, value))
			next(iter);
	}

	template<typename comparator>
	void upper_bound(void *value, comparator cmp, parametric_fileset_iterator *iter)
	{
		if(root_idx == -1)
		{
			iter->idx = -1;
			return iter;
		}
		iterator(root_idx, iter);
		while(true)
		{
			if(!cmp(value, iter->node.value))
			{
				if(iter->node.right != -1)
					iterator(iter->node.right, iter);
				else break;
			}
			else
			{
				if(iter->node.left != -1)
					iterator(iter.node.left, iter);
				else break;
			}
		}
		while(iter->idx != -1 && cmp(value, iter->node.value))
			prev(iter);
	}

	template<typename comparator>
	void find(void *value, comparator cmp, parametric_fileset_iterator *iter)
	{
		lower_bound(value, cmp, iter);
		if(iter->idx == -1 || cmp(value, iter->node.value))
			iter->idx = -1;
	}

	void rotate_left(parametric_fileset_iterator &iter)
	{
		parametric_fileset_iterator t(element_size);
		iterator(iter.node.right, &t);
		iter.node.right = t.node.left;
		parametric_fileset_iterator r(element_size);
		iterator(iter.node.right, &r);
		if(r.idx != -1)
			r.node.upper = iter.idx;
		t.node.left = iter.idx;
		t.node.upper = iter.node.upper;
		iter.node.upper = t.idx;
		if(t.node.upper == -1)
		{
			root_idx = t.idx;
			storage.position = 0;
			storage.write(&root_idx, 8);
		}
		else
		{
			parametric_fileset_iterator u(element_size);
			iterator(t.node.upper, &u);
			if(u.node.left == iter.idx)
				u.node.left = t.idx;
			else u.node.right = t.idx;
			save_node(u);
		}
		save_node(t);
		save_node(iter);
		save_node(r);
	}

	void rotate_right(parametric_fileset_iterator &iter)
	{
		parametric_fileset_iterator t(element_size);
		iterator(iter.node.left, &t);
		iter.node.left = t.node.right;
		parametric_fileset_iterator l(element_size);
		iterator(iter.node.left, &l);
		if(l.idx != -1)
			l.node.upper = iter.idx;
		t.node.right = iter.idx;
		t.node.upper = iter.node.upper;
		iter.node.upper = t.idx;
		if(t.node.upper == -1)
		{
			root_idx = t.idx;
			storage.position = 0;
			storage.write(&root_idx, 8);
		}
		else
		{
			parametric_fileset_iterator u(element_size);
			iterator(t.node.upper, &u);
			if(u.node.left == iter.idx)
				u.node.left = t.idx;
			else u.node.right = t.idx;
			save_node(u);
		}
		save_node(t);
		save_node(iter);
		save_node(l);
	}

	void rebalance(parametric_fileset_iterator &iter)
	{
		if(iter.node.diff == -2)
		{
			parametric_fileset_iterator r(element_size);
			iterator(iter.node.right, &r);
			if(r.node.diff != 1)
			{
				if(r.node.diff == -1)
				{
					iter.node.diff = 0;
					r.node.diff = 0;
				}
				else
				{
					iter.node.diff = -1;
					r.node.diff = 1;
				}
				save_node(iter);
				save_node(r);
				rotate_left(iter);
			}
			else
			{
				parametric_fileset_iterator rl(element_size);
				iterator(r.node.left, &rl);
				if(rl.node.diff == -1)
				{
					iter.node.diff = 1;
					r.node.diff = 0;
				}
				else if(rl.node.diff == 0)
				{
					iter.node.diff = 0;
					r.node.diff = 0;
				}
				else
				{
					iter.node.diff = 0;
					r.node.diff = -1;
				}
				rl.node.diff = 0;
				save_node(iter);
				save_node(r);
				save_node(rl);
				rotate_right(r);
				iterator(iter.idx, &iter);
				rotate_left(iter);
			}
		}
		else
		{
			parametric_fileset_iterator l(element_size);
			iterator(iter.node.left, &l);
			if(l.node.diff != -1)
			{
				if(l.node.diff == 1)
				{
					iter.node.diff = 0;
					l.node.diff = 0;
				}
				else
				{
					iter.node.diff = 1;
					l.node.diff = -1;
				}
				save_node(iter);
				save_node(l);
				rotate_right(iter);
			}
			else
			{
				parametric_fileset_iterator lr(element_size);
				iterator(l.node.right, &lr);
				if(lr.node.diff == 1)
				{
					iter.node.diff = -1;
					l.node.diff = 0;
				}
				else if(lr.node.diff == 0)
				{
					iter.node.diff = 0;
					l.node.diff = 0;
				}
				else
				{
					iter.node.diff = 0;
					l.node.diff = 1;
				}
				lr.node.diff = 0;
				save_node(iter);
				save_node(l);
				save_node(lr);
				rotate_left(l);
				iterator(iter.idx, &iter);
				rotate_right(iter);
			}
		}
	}

	template<typename comparator>
	void insert(void *value, comparator cmp)
	{
		parametric_fileset_iterator node(element_size), new_node(element_size);
		if(free_idx == -1)
		{
			new_node.idx = (storage.size - 32) / (sizeof(parametric_fileset_node) - sizeof(void *) + element_size);
			storage.resize(storage.size + sizeof(parametric_fileset_node) - sizeof(void *) + element_size);
		}
		else
		{
			new_node.idx = free_idx;
			iterator_no_data(free_idx, &node);
			free_idx = node.node.left;
			storage.position = 8;
			storage.write(&free_idx, 8);
		}
		copy_memory(value, new_node.node.value, element_size);
		new_node.node.left = -1;
		new_node.node.right = -1;
		new_node.node.diff = 0;
		size++;
		storage.position = 16;
		storage.write(&size, 8);
		if(root_idx == -1)
		{
			root_idx = new_node.idx;
			storage.position = 0;
			storage.write(&root_idx, 8);
			new_node.node.upper = -1;
		}
		else
		{
			iterator(root_idx, &node);
			while(true)
			{
				if(cmp(value, node.node.value))
				{
					if(node.node.left != -1)
						iterator(node.node.left, &node);
					else
					{
						node.node.left = new_node.idx;
						save_node(node);
						break;
					}
				}
				else
				{
					if(node.node.right != -1)
						iterator(node.node.right, &node);
					else
					{
						node.node.right = new_node.idx;
						save_node(node);
						break;
					}
				}
			}
			new_node.node.upper = node.idx;
		}
		save_node(new_node);
		node.idx = new_node.idx;
		node.node.upper = new_node.node.upper;
		node.node.left = new_node.node.left;
		node.node.right = new_node.node.right;
		node.node.diff = new_node.node.diff;
		int64 prev_idx;
		while(true)
		{
			prev_idx = node.idx;
			iterator(node.node.upper, &node);
			if(node.idx == -1) break;
			if(node.node.left == prev_idx)
				node.node.diff++;
			else node.node.diff--;
			save_node(node);
			if(node.node.diff == 0) break;
			else if(node.node.diff == 2 || node.node.diff == -2)
			{
				rebalance(node);
				iterator(node.node.upper, &node);
				if(node.node.diff == 0) break;
			}
		}
	}

	void remove(parametric_fileset_iterator *iter)
	{
		parametric_fileset_iterator swap_node(element_size), next_iter(element_size);
		size--;
		storage.position = 16;
		storage.write(&size, 8);
		next_iter.idx = iter->idx;
		next_iter.node.upper = iter->node.upper;
		next_iter.node.left = iter->node.left;
		next_iter.node.right = iter->node.right;
		next_iter.node.diff = iter->node.diff;
		next(&next_iter);
		if(iter->node.left != -1 || iter->node.right != -1)
		{
			swap_node.idx = iter->idx;
			swap_node.node.upper = iter->node.upper;
			swap_node.node.left = iter->node.left;
			swap_node.node.right = iter->node.right;
			swap_node.node.diff = iter->node.diff;
			prev(&swap_node);
			if(swap_node.idx == -1)
			{
				parametric_fileset_iterator r(element_size);
				iterator(iter->node.right, &r);
				copy_memory(r.node.value, iter->node.value, element_size);
				save_node(*iter);
				iter->idx = r.idx;
				iter->node.upper = r.node.upper;
				iter->node.left = r.node.left;
				iter->node.right = r.node.right;
				iter->node.diff = r.node.diff;
			}
			else
			{
				copy_memory(swap_node.node.value, iter->node.value, element_size);
				save_node(*iter);
				if(swap_node.node.left != -1)
				{
					parametric_fileset_iterator l(element_size);
					iterator(swap_node.node.left, &l);
					copy_memory(l.node.value, swap_node.node.value, element_size);
					save_node(swap_node);
					iter->idx = l.idx;
					iter->node.upper = l.node.upper;
					iter->node.left = l.node.left;
					iter->node.right = l.node.right;
					iter->node.diff = l.node.diff;
				}
				else
				{
					iter->idx = swap_node.idx;
					iter->node.upper = swap_node.node.upper;
					iter->node.left = swap_node.node.left;
					iter->node.right = swap_node.node.right;
					iter->node.diff = swap_node.node.diff;
				}
			}
		}
		if(iter->node.upper == -1)
		{
			clear();
			next_iter.idx = -1;
			return;
		}
		else
		{
			parametric_fileset_iterator u(element_size);
			iterator(iter->node.upper, &u);
			if(u.node.right == iter->idx)
			{
				u.node.right = -1;
				u.node.diff++;
			}
			else
			{
				u.node.left = -1;
				u.node.diff--;
			}
			save_node(u);
			swap_node.idx = u.idx;
			swap_node.node.upper = u.node.upper;
			swap_node.node.left = u.node.left;
			swap_node.node.right = u.node.right;
			swap_node.node.diff = u.node.diff;
			iter->node.left = free_idx;
			save_node(*iter);
			free_idx = iter->idx;
			storage.position = 8;
			storage.write(&free_idx, 8);
			iter->idx = swap_node.idx;
			iter->node.upper = swap_node.node.upper;
			iter->node.left = swap_node.node.left;
			iter->node.right = swap_node.node.right;
			iter->node.diff = swap_node.node.diff;
		}
		int64 prev_idx;
		while(true)
		{
			
			if(iter->node.diff == 1 || iter->node.diff == -1) break;
			else if(iter->node.diff == 2 || iter->node.diff == -2)
			{
				rebalance(*iter);
				iterator(iter->node.upper, iter);
				if(iter->node.diff != 0) break;
			}
			prev_idx = iter->idx;
			iterator(iter->node.upper, iter);
			if(iter->idx == -1) break;
			if(iter->node.left == prev_idx)
				iter->node.diff--;
			else iter->node.diff++;
			save_node(*iter);
		}
		iterator(next_iter.idx, iter);
	}

	template<typename comparator>
	void remove_value(void *value, comparator cmp)
	{
		parametric_fileset_iterator node(element_size);
		while(true)
		{
			find(value, cmp, &node);
			if(node.idx == -1) break;
			remove(&node);
		}
	}

	void clear()
	{
		storage.resize(32);
		storage.position = 0;
		root_idx = -1;
		storage.write(&root_idx, 8);
		free_idx = -1;
		storage.write(&free_idx, 8);
		size = 0;
		storage.write(&size, 8);
		storage.write(&element_size, 8);
	}
};
