#include "database.h"
#include "json.h"
#include "file.h"
#include "fileset.h"

uint64 data_field_desc::bytes_size()
{
	static uint64 simple_data_field_size[] =
	{
		1,
		1,
		2,
		2,
		4,
		4,
		8,
		8,
		4,
		8
	};
	if(type == data_field_type::u8string || type == data_field_type::binary) return count + 8;
	else if(type == data_field_type::u16string) return count * 2 + 8;
	else if(type == data_field_type::u32string) return count * 4 + 8;
	else return simple_data_field_size[uint64(type)];
}

void data_field::write(data_field_desc &desc, void *addr)
{
	static void(*write_to_raw[])(void *addr, data_field *field, uint64 count) =
	{
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((int8 *)(addr)) = int8(field->i64value);
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((uint8 *)(addr)) = uint8(field->ui64value);
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((int16 *)(addr)) = int16(field->i64value);
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((uint16 *)(addr)) = uint16(field->ui64value);
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((int32 *)(addr)) = int32(field->i64value);
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((uint32 *)(addr)) = uint32(field->ui64value);
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((int64 *)(addr)) = field->i64value;
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((uint64 *)(addr)) = field->ui64value;
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((float32 *)(addr)) = float32(field->f64value);
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((float64 *)(addr)) = field->f64value;
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((uint64 *)(addr)) = field->u8str.size;
			shift_addr(&addr, 8);
			copy_memory(field->u8str.addr, addr, min(field->u8str.size, count));
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((uint64 *)(addr)) = field->u16str.size;
			shift_addr(&addr, 8);
			copy_memory(field->u16str.addr, addr, min(field->u16str.size, count) * 2);
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((uint64 *)(addr)) = field->u32str.size;
			shift_addr(&addr, 8);
			copy_memory(field->u32str.addr, addr, min(field->u32str.size, count) * 4);
		},
		[] (void *addr, data_field *field, uint64 count) -> void
		{
			*((uint64 *)(addr)) = field->binary.size;
			shift_addr(&addr, 8);
			copy_memory(field->binary.addr, addr, min(field->binary.size, count));
		},
	};
	write_to_raw[uint64(desc.type)](addr, this, desc.count);
}

void update_table_cache(database_table *tbl)
{
	tbl->cacheed_indices.clear();
	tbl->cached_offsets.clear();
	uint64 offset = 0;
	for(uint64 i = 0; i < tbl->fields.size; i++)
	{
		tbl->cached_offsets.push(offset);
		if(tbl->fields[i].indexed)
			tbl->cacheed_indices.push(i);
		offset += tbl->fields[i].bytes_size();
	}
	tbl->cached_element_size = offset;
}

struct field_comparator
{
	data_field_desc *desc;
	uint64 offset;

	bool operator()(void *value1, void *value2)
	{
		static bool(*cmp[])(void *a, void *b) =
		{
			[] (void *a, void *b) -> bool
			{
				return *((int8 *)(a)) < *((int8 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((uint8 *)(a)) < *((uint8 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((int16 *)(a)) < *((int16 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((uint16 *)(a)) < *((uint16 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((int32 *)(a)) < *((int32 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((uint32 *)(a)) < *((uint32 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((int64 *)(a)) < *((int64 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((uint64 *)(a)) < *((uint64 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((float32 *)(a)) < *((float32 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((float64 *)(a)) < *((float64 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				uint64 lena = *((uint64 *)(a)), lenb = *((uint64 *)(b)), len = min(lena, lenb);
				shift_addr(&a, 8);
				shift_addr(&b, 8);
				char8 *str1 = (char8 *)(a), *str2 = (char8 *)(b);
				for(uint64 i = 0; i < len; i++)
					if(str1[i] < str2[i]) return true;
					else if(str1[i] > str2[i]) return false;
				return lena < lenb;
			},
			[] (void *a, void *b) -> bool
			{
				uint64 lena = *((uint64 *)(a)), lenb = *((uint64 *)(b)), len = min(lena, lenb);
				shift_addr(&a, 8);
				shift_addr(&b, 8);
				char16 *str1 = (char16 *)(a), *str2 = (char16 *)(b);
				for(uint64 i = 0; i < len; i++)
					if(str1[i] < str2[i]) return true;
					else if(str1[i] > str2[i]) return false;
				return lena < lenb;
			},
			[] (void *a, void *b) -> bool
			{
				uint64 lena = *((uint64 *)(a)), lenb = *((uint64 *)(b)), len = min(lena, lenb);
				shift_addr(&a, 8);
				shift_addr(&b, 8);
				char32 *str1 = (char32 *)(a), *str2 = (char32 *)(b);
				for(uint64 i = 0; i < len; i++)
					if(str1[i] < str2[i]) return true;
					else if(str1[i] > str2[i]) return false;
				return lena < lenb;
			},
			[] (void *a, void *b) -> bool
			{
				uint64 lena = *((uint64 *)(a)), lenb = *((uint64 *)(b)), len = min(lena, lenb);
				shift_addr(&a, 8);
				shift_addr(&b, 8);
				byte *str1 = (byte *)(a), *str2 = (byte *)(b);
				for(uint64 i = 0; i < len; i++)
					if(str1[i] < str2[i]) return true;
					else if(str1[i] > str2[i]) return false;
				return lena < lenb;
			},
		};
		return cmp[uint64(desc->type)]((byte *)(value1) + offset, (byte *)(value2) + offset);
	}
};

struct field_comparator_equality
{
	data_field_desc *desc;
	uint64 offset;

	bool operator()(void *value1, void *value2)
	{
		static bool(*cmp[])(void *a, void *b) =
		{
			[] (void *a, void *b) -> bool
			{
				return *((int8 *)(a)) == *((int8 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((uint8 *)(a)) == *((uint8 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((int16 *)(a)) == *((int16 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((uint16 *)(a)) == *((uint16 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((int32 *)(a)) == *((int32 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((uint32 *)(a)) == *((uint32 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((int64 *)(a)) == *((int64 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((uint64 *)(a)) == *((uint64 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((float32 *)(a)) == *((float32 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				return *((float64 *)(a)) == *((float64 *)(b));
			},
			[] (void *a, void *b) -> bool
			{
				uint64 lena = *((uint64 *)(a)), lenb = *((uint64 *)(b));
				if(lena != lenb) return false;
				shift_addr(&a, 8);
				shift_addr(&b, 8);
				char8 *str1 = (char8 *)(a), *str2 = (char8 *)(b);
				for(uint64 i = 0; i < lena; i++)
					if(str1[i] != str2[i]) return false;
				return true;
			},
			[] (void *a, void *b) -> bool
			{
				uint64 lena = *((uint64 *)(a)), lenb = *((uint64 *)(b));
				if(lena != lenb) return false;
				shift_addr(&a, 8);
				shift_addr(&b, 8);
				char16 *str1 = (char16 *)(a), *str2 = (char16 *)(b);
				for(uint64 i = 0; i < lena; i++)
					if(str1[i] != str2[i]) return false;
				return true;
			},
			[] (void *a, void *b) -> bool
			{
				uint64 lena = *((uint64 *)(a)), lenb = *((uint64 *)(b));
				if(lena != lenb) return false;
				shift_addr(&a, 8);
				shift_addr(&b, 8);
				char32 *str1 = (char32 *)(a), *str2 = (char32 *)(b);
				for(uint64 i = 0; i < lena; i++)
					if(str1[i] != str2[i]) return false;
				return true;
			},
			[] (void *a, void *b) -> bool
			{
				uint64 lena = *((uint64 *)(a)), lenb = *((uint64 *)(b));
				if(lena != lenb) return false;
				shift_addr(&a, 8);
				shift_addr(&b, 8);
				byte *str1 = (byte *)(a), *str2 = (byte *)(b);
				for(uint64 i = 0; i < lena; i++)
					if(str1[i] != str2[i]) return false;
				return true;
			},
		};
		return cmp[uint64(desc->type)]((byte *)(value1) + offset, (byte *)(value2) + offset);
	}
};

void add_to_fileset(void *value, data_field_desc &index_desc, uint64 index_offset, parametric_fileset *fs)
{
	field_comparator fc;
	fc.desc = &index_desc;
	fc.offset = index_offset;
	fs->insert(value, fc);
}


void database::create(db_result *dbr)
{
	commit(dbr);
}

void database::load(db_result *dbr)
{
	file f;
	f.filename << path << u'\\' << name << u".json";
	if(!f.exists())
	{
		dbr->success = false;
		dbr->msg <<= u"Could not open database main file.";
		return;
	}
	f.open();
	if(f.status != filestatus::opened)
	{
		dbr->success = false;
		dbr->msg <<= u"Could not open database main file.";
		return;
	}
	u16string dbstr;
	dbstr.insert_default(0, f.size / 2);
	f.read(f.size, dbstr.addr);
	json<char16> db;
	json_parse_result result = db.load(dbstr);
	if(!result.success)
	{
		dbr->success = false;
		dbr->msg.clear();
		dbr->msg << u"Could not parse database main json file, error in line: " << result.line << u", position: " << result.position << ".";
		return;
	}
	if(db.root.type != json_node_type::array)
	{
		dbr->success = false;
		dbr->msg <<= u"Database main json file is corrupted.";
		return;
	}
	for(uint64 i = 0; i < db.root.values.size; i++)
	{
		if(db.root.values[i].type != json_node_type::object
			|| db.root.values[i].values.size != 2
			|| db.root.values[i].values[0].type != json_node_type::string
			|| db.root.values[i].values[0].key != u"name"
			|| db.root.values[i].values[1].type != json_node_type::array
			|| db.root.values[i].values[1].key != u"fields")
		{
			dbr->success = false;
			dbr->msg <<= u"Database main json file is corrupted.";
			return;
		}
		tables.push_default();
		tables.back().name << db.root.values[i].values[0].string_value;
		for(uint64 j = 0; j < db.root.values[i].values[1].values.size; j++)
		{
			if(db.root.values[i].values[1].values[j].type != json_node_type::object
				|| db.root.values[i].values[1].values[j].values.size != 4
				|| db.root.values[i].values[1].values[j].values[0].type != json_node_type::string
				|| db.root.values[i].values[1].values[j].values[0].key != u"name"
				|| db.root.values[i].values[1].values[j].values[1].type != json_node_type::numeric
				|| db.root.values[i].values[1].values[j].values[1].key != u"type"
				|| db.root.values[i].values[1].values[j].values[2].type != json_node_type::numeric
				|| db.root.values[i].values[1].values[j].values[2].key != u"count"
				|| db.root.values[i].values[1].values[j].values[3].type != json_node_type::boolean
				|| db.root.values[i].values[1].values[j].values[3].key != u"indexed")
			{
				dbr->success = false;
				dbr->msg <<= u"Database main json file is corrupted.";
				return;
			}
			tables.back().fields.push_default();
			tables.back().fields.back().name << db.root.values[i].values[1].values[j].values[0].string_value;
			tables.back().fields.back().type = data_field_type(db.root.values[i].values[1].values[j].values[1].numeric_value);
			tables.back().fields.back().count = uint64(db.root.values[i].values[1].values[j].values[2].numeric_value);
			tables.back().fields.back().indexed = db.root.values[i].values[1].values[j].values[3].boolean_value;
		}
		update_table_cache(&tables.back());
	}
	dbr->success = true;
}

void database::commit(db_result *dbr)
{
	json<char16> db;
	db.root.type = json_node_type::array;
	for(uint64 i = 0; i < tables.size; i++)
	{
		db.root.values.push_default();
		db.root.values.back().type = json_node_type::object;
		db.root.values.back().values.push_default();
		db.root.values.back().values[0].type = json_node_type::string;
		db.root.values.back().values[0].key << u"name";
		db.root.values.back().values[0].string_value << tables[i].name;
		db.root.values.back().values.push_default();
		db.root.values.back().values[1].type = json_node_type::array;
		db.root.values.back().values[1].key << u"fields";
		for(uint64 j = 0; j < tables[i].fields.size; j++)
		{
			db.root.values.back().values[1].values.push_default();
			db.root.values.back().values[1].values.back().type = json_node_type::object;
			db.root.values.back().values[1].values.back().values.push_default();
			db.root.values.back().values[1].values.back().values[0].type = json_node_type::string;
			db.root.values.back().values[1].values.back().values[0].key << "name";
			db.root.values.back().values[1].values.back().values[0].string_value << tables[i].fields[j].name;
			db.root.values.back().values[1].values.back().values.push_default();
			db.root.values.back().values[1].values.back().values[1].type = json_node_type::numeric;
			db.root.values.back().values[1].values.back().values[1].key << "type";
			db.root.values.back().values[1].values.back().values[1].numeric_value = float64(tables[i].fields[j].type);
			db.root.values.back().values[1].values.back().values.push_default();
			db.root.values.back().values[1].values.back().values[2].type = json_node_type::numeric;
			db.root.values.back().values[1].values.back().values[2].key << "count";
			db.root.values.back().values[1].values.back().values[2].numeric_value = float64(tables[i].fields[j].count);
			db.root.values.back().values[1].values.back().values.push_default();
			db.root.values.back().values[1].values.back().values[3].type = json_node_type::boolean;
			db.root.values.back().values[1].values.back().values[3].key << "indexed";
			db.root.values.back().values[1].values.back().values[3].boolean_value = tables[i].fields[j].indexed;
		}
	}
	file f;
	f.filename << path << u'\\' << name << u".json";
	f.open();
	if(f.status != filestatus::opened)
	{
		dbr->success = false;
		dbr->msg <<= u"Unnable to open database main file";
	}
	u16string dbstr;
	db.stringify(&dbstr);
	if(f.write(dbstr.addr, dbstr.size * sizeof(char16)) != dbstr.size * sizeof(char16))
	{
		dbr->success = false;
		dbr->msg <<= u"Unnable to overwrite database main file";
	}
	f.close();
	dbr->success = true;
}

void database::create_table(u16string &name, db_result *dbr)
{
	if(table_index(name) != not_exists<uint64>)
	{
		dbr->success = false;
		dbr->msg <<= u"Table already exists";
		return;
	}
	tables.push_default();
	tables.back().name << name;
	dbr->success = true;
	commit(dbr);
}

void database::remove_table(uint64 table_idx, db_result *dbr)
{
	array<file> f;
	for(uint64 i = 0; i < tables[table_idx].cacheed_indices.size; i++)
	{
		f.push_default();
		f[i].filename << path << u'\\' << name << u'_' << tables[table_idx].name << u'_'
			<< tables[table_idx].fields[tables[table_idx].cacheed_indices[i]].name << u".dbdata";
		f[i].open();
		if(f[i].status != filestatus::opened)
		{
			dbr->success = false;
			dbr->msg <<= u"Could not open table data file.";
			return;
		}
	}
	for(uint64 i = 0; i < tables[table_idx].cacheed_indices.size; i++)
		f[i].remove();
	tables.remove(table_idx);
	dbr->success = true;
	commit(dbr);
}

uint64 database::table_index(u16string &name)
{
	for(uint64 i = 0; i < tables.size; i++)
		if(tables[i].name == name) return i;
	return not_exists<uint64>;
}

uint64 database::column_index(uint64 table_idx, u16string &name)
{
	for(uint64 i = 0; i < tables[table_idx].fields.size; i++)
		if(tables[table_idx].fields[i].name == name) return i;
	return not_exists<uint64>;
}

void database::add_column(uint64 table_idx, uint64 insert_idx, data_field_desc &desc, data_field &fill_value, db_result *dbr)
{
	array<parametric_fileset> oldf, newf;
	array<data_field_desc *> old_fields, new_fields;
	for(uint64 i = 0; i < tables[table_idx].fields.size; i++)
	{
		old_fields.push(&tables[table_idx].fields[i]);
		new_fields.push(&tables[table_idx].fields[i]);
	}
	new_fields.insert(insert_idx, &desc);
	uint64 move_offset = 0, offset = 0;
	array<uint64> newo;
	array<data_field_desc *> newi;
	for(uint64 i = 0; i < old_fields.size; i++)
	{
		if(insert_idx == i) move_offset = offset;
		if(old_fields[i]->indexed)
		{
			oldf.push_default();
			oldf.back().storage.filename << path << u'\\' << name << u'_'
				<< tables[table_idx].name << u'_' << old_fields[i]->name << u".dbdata";
			oldf.back().element_size = tables[table_idx].cached_element_size;
			oldf.back().open();
			if(oldf.back().storage.status != filestatus::opened)
			{
				dbr->success = false;
				dbr->msg <<= u"Could not open table data file";
				return;
			}
		}
		offset += old_fields[i]->bytes_size();
	}
	if(insert_idx == old_fields.size) move_offset = offset;
	offset = 0;
	for(uint64 i = 0; i < new_fields.size; i++)
	{
		if(new_fields[i]->indexed)
		{
			newo.push(offset);
			newi.push(new_fields[i]);
			newf.push_default();
			newf.back().storage.filename << path << u'\\' << name << u'_'
				<< tables[table_idx].name << u'_' << new_fields[i]->name << u".dbdatatemp";
			
		}
		offset += new_fields[i]->bytes_size();
	}
	for(uint64 i = 0; i < newf.size; i++)
	{
		newf[i].element_size = offset;
		newf[i].open();
		if(newf[i].storage.status != filestatus::opened)
		{
			dbr->success = false;
			dbr->msg <<= u"Could not open table data file";
			return;
		}
	}
	tables[table_idx].fields.insert_default(insert_idx, 1);
	tables[table_idx].fields[insert_idx].name << desc.name;
	tables[table_idx].fields[insert_idx].type = desc.type;
	tables[table_idx].fields[insert_idx].count = desc.count;
	tables[table_idx].fields[insert_idx].indexed = desc.indexed;
	update_table_cache(&tables[table_idx]);
	if(oldf.size != 0)
	{
		parametric_fileset_iterator iter(oldf.back().element_size);
		oldf.back().begin(&iter);
		void *new_data = new byte[newf[0].element_size];
		while(iter.idx != not_exists<int64>)
		{
			copy_memory(iter.node.value, new_data, oldf.back().element_size);
			move_memory(
				(byte *)(new_data) + move_offset,
				(byte *)(new_data) + move_offset + desc.bytes_size(),
				oldf.back().element_size - move_offset);
			fill_value.write(desc, (byte *)(new_data) + move_offset);
			for(uint64 j = 0; j < newf.size; j++)
				add_to_fileset(new_data, *newi[j], newo[j], &newf[j]);
			oldf.back().next(&iter);
		}
		delete[] new_data;
	}
	for(uint64 i = 0; i < oldf.size; i++)
		oldf[i].storage.remove();
	u16string p;
	for(uint64 i = 0; i < newf.size; i++)
	{
		p <<= newf[i].storage.filename;
		p.remove_range(p.size - 4, p.size);
		newf[i].storage.move(p);
	}
	dbr->success = true;
	commit(dbr);
}

void database::remove_column(uint64 table_idx, uint64 column_idx, db_result *dbr)
{
	array<parametric_fileset> oldf, newf;
	array<data_field_desc *> old_fields, new_fields;
	for(uint64 i = 0; i < tables[table_idx].fields.size; i++)
	{
		old_fields.push(&tables[table_idx].fields[i]);
		new_fields.push(&tables[table_idx].fields[i]);
	}
	new_fields.remove(column_idx);
	uint64 move_offset = 0, offset = 0;
	array<uint64> newo;
	array<data_field_desc *> newi;
	for(uint64 i = 0; i < old_fields.size; i++)
	{
		if(column_idx == i) move_offset = offset;
		if(old_fields[i]->indexed)
		{
			if(column_idx == i)
			{
				file f;
				f.filename << path << u'\\' << name << u'_'
					<< tables[table_idx].name << u'_' << old_fields[i]->name << u".dbdata";
				f.remove();
			}
			else
			{
				oldf.push_default();
				oldf.back().storage.filename << path << u'\\' << name << u'_'
					<< tables[table_idx].name << u'_' << old_fields[i]->name << u".dbdata";
				oldf.back().element_size = tables[table_idx].cached_element_size;
				oldf.back().open();
				if(oldf.back().storage.status != filestatus::opened)
				{
					dbr->success = false;
					dbr->msg <<= u"Could not open table data file";
					return;
				}
			}
		}
		offset += old_fields[i]->bytes_size();
	}
	offset = 0;
	for(uint64 i = 0; i < new_fields.size; i++)
	{
		if(new_fields[i]->indexed)
		{
			newo.push(offset);
			newi.push(new_fields[i]);
			newf.push_default();
			newf.back().storage.filename << path << u'\\' << name << u'_'
				<< tables[table_idx].name << u'_' << new_fields[i]->name << u".dbdatatemp";
		}
		offset += new_fields[i]->bytes_size();
	}
	for(uint64 i = 0; i < newf.size; i++)
	{
		newf[i].element_size = offset;
		newf[i].open();
		if(newf[i].storage.status != filestatus::opened)
		{
			dbr->success = false;
			dbr->msg <<= u"Could not open table data file";
			return;
		}
	}
	if(oldf.size != 0)
	{
		parametric_fileset_iterator iter(oldf.back().element_size);
		oldf.back().begin(&iter);
		void *new_data = new byte[newf[0].element_size];
		while(iter.idx != not_exists<int64>)
		{
			move_memory(
				(byte *)(iter.node.value) + move_offset + tables[table_idx].fields[column_idx].bytes_size(),
				(byte *)(iter.node.value) + move_offset,
				oldf.back().element_size - (move_offset + tables[table_idx].fields[column_idx].bytes_size()));
			copy_memory(iter.node.value, new_data, newf[0].element_size);
			for(uint64 j = 0; j < newf.size; j++)
				add_to_fileset(new_data, *newi[j], newo[j], &newf[j]);
			oldf.back().next(&iter);
		}
		delete[] new_data;
	}
	tables[table_idx].fields.remove(column_idx);
	update_table_cache(&tables[table_idx]);
	for(uint64 i = 0; i < oldf.size; i++)
		oldf[i].storage.remove();
	u16string p;
	for(uint64 i = 0; i < newf.size; i++)
	{
		p <<= newf[i].storage.filename;
		p.remove_range(p.size - 4, p.size);
		newf[i].storage.move(p);
	}
	dbr->success = true;
	commit(dbr);
}

bool database::insert_row(uint64 table_idx, void *value)
{
	array<parametric_fileset> fs;
	for(uint64 i = 0; i < tables[table_idx].cacheed_indices.size; i++)
	{
		fs.push_default();
		fs[i].storage.filename << path << u'\\' << name << u'_' << tables[table_idx].name << u'_'
			<< tables[table_idx].fields[tables[table_idx].cacheed_indices[i]].name << u".dbdata";
		fs[i].element_size = tables[table_idx].cached_element_size;
		fs[i].open();
		if(fs[i].storage.status != filestatus::opened) return false;
	}
	for(uint64 i = 0; i < tables[table_idx].cacheed_indices.size; i++)
		add_to_fileset(
			value,
			tables[table_idx].fields[tables[table_idx].cacheed_indices[i]],
			tables[table_idx].cached_offsets[tables[table_idx].cacheed_indices[i]],
			&fs[i]);
	return true;
}

void database::query_rows(uint64 table_idx, const function<row_action(void *row_data)> &callback)
{
	if(tables[table_idx].cacheed_indices.size == 0) return;
	parametric_fileset fs;
	fs.storage.filename << path << u'\\' << name << u'_' << tables[table_idx].name << u'_'
		<< tables[table_idx].fields[tables[table_idx].cacheed_indices[0]].name << u".dbdata";
	fs.element_size = tables[table_idx].cached_element_size;
	fs.open();
	if(fs.storage.status != filestatus::opened) return;
	parametric_fileset_iterator iter(fs.element_size);
	row_action ra;
	fs.begin(&iter);
	while(iter.idx != not_exists<int64>)
	{
		ra = callback(iter.node.value);
		if(ra == row_action::no_change) fs.next(&iter);
		else if(ra == row_action::update)
		{
			fs.save_node(iter);
			fs.next(&iter);
		}
		else if(ra == row_action::remove) fs.remove(&iter);
		else break;
	}
}

void database::query_matched_rows(uint64 table_idx, uint64 column_idx, data_field &key, const function<row_action(void *row_data)> &callback)
{
	parametric_fileset fs;
	if(tables[table_idx].fields[column_idx].indexed)
	{
		fs.storage.filename << path << u'\\' << name << u'_' << tables[table_idx].name << u'_'
			<< tables[table_idx].fields[column_idx].name << u".dbdata";
		fs.element_size = tables[table_idx].cached_element_size;
		fs.open();
		if(fs.storage.status != filestatus::opened) return;
		parametric_fileset_iterator iter(fs.element_size);
		byte *value = new byte[fs.element_size];
		key.write(tables[table_idx].fields[column_idx], value + tables[table_idx].cached_offsets[column_idx]);
		field_comparator fc;
		fc.desc = &tables[table_idx].fields[column_idx];
		fc.offset = tables[table_idx].cached_offsets[column_idx];
		row_action ra;
		fs.lower_bound(value, fc, &iter);
		while(iter.idx != not_exists<int64> && !fc(value, iter.node.value))
		{
			ra = callback(iter.node.value);
			if(ra == row_action::no_change) fs.next(&iter);
			else if(ra == row_action::update)
			{
				fs.save_node(iter);
				fs.next(&iter);
			}
			else if(ra == row_action::remove) fs.remove(&iter);
			else break;
		}
		delete[] value;
	}
	else
	{
		if(tables[table_idx].cacheed_indices.size == 0) return;
		fs.storage.filename << path << u'\\' << name << u'_' << tables[table_idx].name << u'_'
			<< tables[table_idx].fields[tables[table_idx].cacheed_indices[0]].name << u".dbdata";
		fs.element_size = tables[table_idx].cached_element_size;
		fs.open();
		if(fs.storage.status != filestatus::opened) return;
		parametric_fileset_iterator iter(fs.element_size);
		byte *value = new byte[fs.element_size];
		key.write(tables[table_idx].fields[column_idx], value + tables[table_idx].cached_offsets[column_idx]);
		field_comparator_equality fc;
		fc.desc = &tables[table_idx].fields[column_idx];
		fc.offset = tables[table_idx].cached_offsets[column_idx];
		row_action ra;
		fs.begin(&iter);
		while(iter.idx != not_exists<int64>)
		{
			if(fc(value, iter.node.value))
			{
				ra = callback(iter.node.value);
				if(ra == row_action::no_change) fs.next(&iter);
				else if(ra == row_action::update)
				{
					fs.save_node(iter);
					fs.next(&iter);
				}
				else if(ra == row_action::remove) fs.remove(&iter);
				else break;
			}
			else fs.next(&iter);
		}
		delete[] value;
	}
}

uint64 database::table_size(uint64 table_idx)
{
	if(tables[table_idx].cacheed_indices.size == 0) return 0;
	parametric_fileset fs;
	fs.storage.filename << path << u'\\' << name << u'_' << tables[table_idx].name << u'_'
		<< tables[table_idx].fields[tables[table_idx].cacheed_indices[0]].name << u".dbdata";
	fs.element_size = tables[table_idx].cached_element_size;
	fs.open();
	return fs.size;
}

void database::split_row(uint64 table_idx, void *value, array<data_field> *columns)
{
	static void(*write_column[])(void *value, data_field *field) =
	{
		[] (void *value, data_field *field) -> void
		{
			field->i64value = int64(*((int8 *)(value)));
		},
		[] (void *value, data_field *field) -> void
		{
			field->ui64value = uint64(*((uint8 *)(value)));
		},
		[] (void *value, data_field *field) -> void
		{
			field->i64value = int64(*((int16 *)(value)));
		},
		[] (void *value, data_field *field) -> void
		{
			field->ui64value = uint64(*((uint16 *)(value)));
		},
		[] (void *value, data_field *field) -> void
		{
			field->i64value = int64(*((int32 *)(value)));
		},
		[] (void *value, data_field *field) -> void
		{
			field->ui64value = uint64(*((uint32 *)(value)));
		},
		[] (void *value, data_field *field) -> void
		{
			field->i64value = *((int64 *)(value));
		},
		[] (void *value, data_field *field) -> void
		{
			field->ui64value = *((uint64 *)(value));
		},
		[] (void *value, data_field *field) -> void
		{
			field->f64value = float64(*((float32 *)(value)));
		},
		[] (void *value, data_field *field) -> void
		{
			field->f64value = *((float64 *)(value));
		},
		[] (void *value, data_field *field) -> void
		{
			uint64 count = *((uint64 *)(value));
			shift_addr(&value, 8);
			field->u8str.insert_range(0, (char8 *)(value), (char8 *)(value) + count);
		},
		[] (void *value, data_field *field) -> void
		{
			uint64 count = *((uint64 *)(value));
			shift_addr(&value, 8);
			field->u16str.insert_range(0, (char16 *)(value), (char16 *)(value) + count);
		},
		[] (void *value, data_field *field) -> void
		{
			uint64 count = *((uint64 *)(value));
			shift_addr(&value, 8);
			field->u32str.insert_range(0, (char32 *)(value), (char32 *)(value) + count);
		},
		[] (void *value, data_field *field) -> void
		{
			uint64 count = *((uint64 *)(value));
			shift_addr(&value, 8);
			field->binary.insert_range(0, (byte *)(value), (byte *)(value) + count);
		},
	};
	for(uint64 i = 0; i < tables[table_idx].fields.size; i++)
	{
		columns->push_default();
		write_column[uint64(tables[table_idx].fields[i].type)](value, &columns->back());
		shift_addr(&value, int64(tables[table_idx].fields[i].bytes_size()));
	}
}
