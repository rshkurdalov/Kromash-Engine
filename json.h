#pragma once
#include "array.h"
#include "string.h"

enum struct json_node_type
{
	string,
	numeric,
	boolean,
	null,
	object,
	array,
};

template<typename char_type> struct json_node
{
	json_node_type type;
	string<char_type> key;
	string<char_type> string_value;
	float64 numeric_value;
	bool boolean_value;
	array<json_node> values;

	uint64 find(const string<char_type> &key_value)
	{
		for(uint64 i = 0; i < values.size; i++)
			if(values[i].key == key_value) return i;
		return not_found<uint64>;
	}

	json_node &operator[](const string<char_type> &key_value)
	{
		for(uint64 i = 0; i < values.size; i++)
			if(values[i].key == key_value) return values[i];
		return *((json_node *)(nullptr));
	}

	json_node &operator[](uint64 idx)
	{
		return values[idx];
	}
};

struct json_parse_result
{
	bool success;
	uint64 line;
	uint64 position;
};

template<typename char_type> struct json_parsing
{
	string<char_type> *input;
	uint64 idx;
	json_node<char_type> *root;
	uint64 line;
	uint64 line_begin;
	bool error;

	void skip_whitespaces()
	{
		while(idx != input->size)
		{
			if(input->addr[idx] == char_type(' ')
				|| input->addr[idx] == char_type('\r')
				|| input->addr[idx] == char_type('\t'))
				idx++;
			else if(input->addr[idx] == char_type('\n'))
			{
				idx++;
				line++;
				line_begin = idx;
			}
			else break;
		}
	}

	void read_word(string<char_type> *word)
	{
		idx++;
		while(idx != input->size && input->addr[idx] != char_type('\"'))
		{
			if(input->addr[idx] == '\\')
			{
				idx++;
				if(idx == input->size)
				{
					error = true;
					return;
				}
				if(input->addr[idx] == char_type('\"'))
				{
					word->push(char_type('\"'));
					idx++;
				}
				else if(input->addr[idx] == char_type('\\'))
				{
					word->push(char_type('\\'));
					idx++;
				}
				else if(input->addr[idx] == char_type('\b'))
				{
					word->push(char_type('\b'));
					idx++;
				}
				else if(input->addr[idx] == char_type('\f'))
				{
					word->push(char_type('\f'));
					idx++;
				}
				else if(input->addr[idx] == char_type('\n'))
				{
					word->push(char_type('\n'));
					idx++;
				}
				else if(input->addr[idx] == char_type('\r'))
				{
					word->push(char_type('\r'));
					idx++;
				}
				else if(input->addr[idx] == char_type('\t'))
				{
					word->push(char_type('\t'));
					idx++;
				}
				else if(input->addr[idx] == char_type('u'))
				{
					idx++;
					if(input->size - idx < 4)
					{
						error = true;
						return;
					}
					int64 ch = load_integer(input->addr, 4, 16);
					word->push(char_type(ch));
					for(uint32 i = 0; i < 4; i++)
					{
						if(input->addr[idx] >= char_type('0')
							&& input->addr[idx] <= char_type('9')
							|| input->addr[idx] >= char_type('a')
							&& input->addr[idx] <= char_type('f')
							|| input->addr[idx] >= char_type('A')
							&& input->addr[idx] <= char_type('F'))
								idx++;
						else
						{
							error = true;
							return;
						}
					}
				}
				else
				{
					error = true;
					return;
				}
			}
			else word->push(input->addr[idx++]);
		}
		if(input->addr[idx] != '\"')
		{
			error = true;
			return;
		}
		idx++;
	}

	void read_number(float64 *number)
	{
		uint64 chars_converted;
		*number = load_float64(input->addr + idx, min(50, input->size - idx), &chars_converted);
		idx += chars_converted;
	}

	void read_object(json_node<char_type> *node)
	{
		idx++;
		skip_whitespaces();
		if(idx == input->size)
		{
			error = true;
			return;
		}
		if(input->addr[idx] == char_type('}'))
		{
			idx++;
			return;
		}
		while(true)
		{
			skip_whitespaces();
			if(input->addr[idx] != char_type('\"'))
			{
				error = true;
				return;
			}
			node->values.push_default();
			read_word(&node->values.back().key);
			if(error) return;
			skip_whitespaces();
			if(input->addr[idx] != char_type(':'))
			{
				error = true;
				return;
			}
			idx++;
			read_value(&node->values.back());
			if(error) return;
			if(idx == input->size)
			{
				error = true;
				return;
			}
			if(input->addr[idx] == char_type('}'))
			{
				idx++;
				return;
			}
			if(input->addr[idx] != char_type(','))
			{
				error = true;
				return;
			}
			idx++;
		}
	}

	void read_array(json_node<char_type> *node)
	{
		idx++;
		skip_whitespaces();
		if(idx == input->size)
		{
			error = true;
			return;
		}
		if(input->addr[idx] == char_type(']'))
		{
			idx++;
			return;
		}
		while(true)
		{
			skip_whitespaces();
			node->values.push_default();
			read_value(&node->values.back());
			if(error) return;
			skip_whitespaces();
			if(idx == input->size)
			{
				error = true;
				return;
			}
			if(input->addr[idx] == char_type(']'))
			{
				idx++;
				return;
			}
			if(input->addr[idx] != char_type(','))
			{
				error = true;
				return;
			}
			idx++;
		}
	}

	void read_value(json_node<char_type> *node)
	{
		skip_whitespaces();
		if(idx == input->size)
		{
			error = true;
			return;
		}
		if(input->addr[idx] == char_type('\"'))
		{
			node->type = json_node_type::string;
			read_word(&node->string_value);
		}
		else if(input->size - idx >= 2 && input->addr[idx] == char_type('-')
			&& input->addr[idx + 1] >= char_type('0') && input->addr[idx + 1] <= char_type('9')
			|| input->addr[idx] >= char_type('0') && input->addr[idx] <= char_type('9'))
		{
			node->type = json_node_type::numeric;
			read_number(&node->numeric_value);
		}
		else if(input->addr[idx] == char_type('{'))
		{
			node->type = json_node_type::object;
			read_object(node);
		}
		else if(input->addr[idx] == char_type('['))
		{
			node->type = json_node_type::array;
			read_array(node);
		}
		else if(input->size - idx >= 4
			&& input->addr[idx] == char_type('t')
			&& input->addr[idx + 1] == char_type('r')
			&& input->addr[idx + 2] == char_type('u')
			&& input->addr[idx + 3] == char_type('e'))
		{
			node->type = json_node_type::boolean;
			node->boolean_value = true;
			idx += 4;
		}
		else if(input->size - idx >= 5
			&& input->addr[idx] == char_type('f')
			&& input->addr[idx + 1] == char_type('a')
			&& input->addr[idx + 2] == char_type('l')
			&& input->addr[idx + 3] == char_type('s')
			&& input->addr[idx + 4] == char_type('e'))
		{
			node->type = json_node_type::boolean;
			node->boolean_value = false;
			idx += 5;
		}
		else if(input->size - idx >= 4
			&& input->addr[idx] == char_type('n')
			&& input->addr[idx + 1] == char_type('u')
			&& input->addr[idx + 2] == char_type('l')
			&& input->addr[idx + 3] == char_type('l'))
		{
			node->type = json_node_type::null;
			idx += 4;
		}
		else
		{
			error = true;
			return;
		}
		if(error) return;
		skip_whitespaces();
	}

	json_parse_result parse()
	{
		json_parse_result result;
		line = 1;
		line_begin = 1;
		error = false;
		skip_whitespaces();
		if(idx != input->size) read_value(root);
		if(error)
		{
			result.success = false;
			result.line = line;
			result.position = idx - line_begin + 1;
		}
		else result.success = true;
		return result;
	}
};

template<typename char_type> struct json_writing
{
	string<char_type> *output;
	json_node<char_type> *root;
	uint32 tabs;

	void write_tabs()
	{
		for(uint32 i = 0; i < tabs; i++)
			*output << char_type('\t');
	}

	void write_value(json_node<char_type> *node)
	{
		if(node->type == json_node_type::array)
		{
			if(node->values.size == 0) *output << "[]";
			else
			{
				*output << "[\n";
				tabs++;
				write_tabs();
				for(uint64 i = 0; i < node->values.size; i++)
				{
					write_value(&node->values[i]);
					if(i != node->values.size - 1)
					{
						*output << ",\n";
						write_tabs();
					}
					else *output << char_type('\n');
				}
				tabs--;
				write_tabs();
				*output << "]";
			}
		}
		else if(node->type == json_node_type::object)
		{
			if(node->values.size == 0) *output << "{}";
			else
			{
				*output << "{\n";
				tabs++;
				write_tabs();
				for(uint64 i = 0; i < node->values.size; i++)
				{
					if(node->values[i].type == json_node_type::array
						|| node->values[i].type == json_node_type::object)
					{
						*output << char_type('\"') << node->values[i].key << "\":\n";
						write_tabs();
					}
					else *output << char_type('\"') << node->values[i].key << "\": ";
					write_value(&node->values[i]);
					if(i != node->values.size - 1)
					{
						*output << ",\n";
						write_tabs();
					}
					else *output << char_type('\n');
				}
				tabs--;
				write_tabs();
				*output << "}";
			}
		}
		else if(node->type == json_node_type::boolean)
		{
			if(node->boolean_value) *output << "true";
			else *output << "false";
		}
		else if(node->type == json_node_type::null)
			*output << "null";
		else if(node->type == json_node_type::numeric)
			write_float64(node->numeric_value, output);
		else if(node->type == json_node_type::string)
		{
			*output << char_type('\"');
			for(uint64 i = 0; i < node->string_value.size; i++)
			{
				if(node->string_value[i] == char_type('\"'))
					*output << "\\\"";
				else if(node->string_value[i] == char_type('\\'))
					*output << "\\\\";
				else if(node->string_value[i] == char_type('/'))
					*output << "\\/";
				else if(node->string_value[i] == char_type('\b'))
					*output << "\\b";
				else if(node->string_value[i] == char_type('\f'))
					*output << "\\f";
				else if(node->string_value[i] == char_type('\n'))
					*output << "\\n";
				else if(node->string_value[i] == char_type('\r'))
					*output << "\\r";
				else if(node->string_value[i] == char_type('\t'))
					*output << "\\t";
				else *output << node->string_value[i];
			}
			*output << char_type('\"');
		}
	}

	void write()
	{
		tabs = 0;
		write_value(root);
	}
};

template<typename char_type> struct json
{
	json_node<char_type> root;

	json_parse_result load(string<char_type> &input)
	{
		json_parsing<char_type> parsing;
		parsing.input = &input;
		parsing.idx = 0;
		parsing.root = &root;
		return parsing.parse();
	}

	void stringify(string<char_type> *output)
	{
		json_writing<char_type> writing;
		writing.output = output;
		writing.root = &root;
		writing.write();
	}
};
