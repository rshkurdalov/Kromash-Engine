#pragma once
#include "string.h"
#include "array.h"
#include "function.h"

enum struct data_field_type : uint32
{
	int8,
	uint8,
	int16,
	uint16,
	int32,
	uint32,
	int64,
	uint64,
	float32,
	float64,
	u8string,
	u16string,
	u32string,
	binary
};

/*On binary, u8string, u16string, u32string fields <count> means number of elements,
on other fields is not used and may be operated as incremental index.
Actual size in bytes of binary, u8string, u16string, u32string fields
in raw data is (number of elements) * (element size) + leading 8 bytes
used for actual array size*/
struct data_field_desc
{
	u16string name;
	data_field_type type;
	uint64 count;
	bool indexed;

	uint64 bytes_size();
};

struct data_field
{
	union
	{
		int64 i64value;
		uint64 ui64value;
		float64 f64value;
	};
	u8string u8str;
	u16string u16str;
	u32string u32str;
	array<byte> binary;

	void write(data_field_desc &desc, void *addr);
};

struct database_table
{
	u16string name;
	array<data_field_desc> fields;
	array<uint64> cacheed_indices;
	array<uint64> cached_offsets;
	uint64 cached_element_size;
};

struct db_result
{
	bool success;
	u16string msg;
};

enum struct row_action
{
	no_change,
	update,
	remove,
	end_iteration
};

struct database
{
	u16string path;
	u16string name;
	array<database_table> tables;

	void create(db_result *dbr);
	void load(db_result *dbr);
	void commit(db_result *dbr);
	void create_table(u16string &name, db_result *dbr);
	void remove_table(uint64 table_idx, db_result *dbr);
	uint64 table_index(u16string &name);
	uint64 column_index(uint64 table_idx, u16string &name);
	void add_column(uint64 table_idx, uint64 insert_idx, data_field_desc &desc, data_field &fill_value, db_result *dbr);
	void remove_column(uint64 table_idx, uint64 column_idx, db_result *dbr);
	bool insert_row(uint64 table_idx, void *value);
	void query_rows(uint64 table_idx, const function<row_action(void *row_data)> &callback);
	void query_matched_rows(uint64 table_idx, uint64 column_idx, data_field &key, const function<row_action(void *row_data)> &callback);
	uint64 table_size(uint64 table_idx);
	void split_row(uint64 table_idx, void *value, array<data_field> *columns);
};
