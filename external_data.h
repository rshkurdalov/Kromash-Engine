#pragma once
#include "string.h"

/*Get length of size-zero string*/
template<typename char_type>
uint64 sz_length(const char_type *str)
{
	uint64 length = 0;
	while(str[length] != char_type('\0')) length++;
	return length;
}

/*Create string from str with zero symbol in the end*/
template<typename char_type>
char8 *create_u8sz(string<char_type> &str)
{
	char8 *addr = new char8[str.size + 1];
	for(uint64 i = 0; i < str.size; i++)
		addr[i] = char8(str.addr[i]);
	addr[str.size] = '\0';
	return addr;
}

/*Create string from str with zero symbol in the end*/
template<typename char_type>
char16 *create_u16sz(string<char_type> &str)
{
	char16 *addr = new char16[str.size + 1];
	for(uint64 i = 0; i < str.size; i++)
		addr[i] = char16(str.addr[i]);
	addr[str.size] = u'\0';
	return addr;
}

/*Create string from str with zero symbol in the end*/
template<typename char_type>
char32 *create_u32sz(string<char_type> &str)
{
	char32 *addr = new char32[str.size + 1];
	for(uint64 i = 0; i < str.size; i++)
		addr[i] = char32(str.addr[i]);
	addr[str.size] = u'\0';
	return addr;
}

/*Append converted string from input to output*/
template<typename input_type, typename output_type>
void convert_string(string<input_type> &input, string<output_type> *output)
{
	if(output->capacity - output->size < input.size)
		output->increase_capacity(input.size - (output->capacity - output->size));
	for(uint64 i = 0; i < input.size; i++)
		*output << output_type(input.addr[i]);
}

/*Append converted string from input to output*/
template<typename input_type, typename output_type>
void convert_sz(const input_type *input, string<output_type> *output)
{
	uint64 length = sz_length(input);
	if(output->capacity - output->size < length)
		output->increase_capacity(length - (output->capacity - output->size));
	for(uint64 i = 0; i < length; i++)
		*output << output_type(input[i]);
}
