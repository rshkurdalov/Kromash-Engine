#pragma once
#include "string.h"

enum struct filestatus
{
	opened,
	closed
};

struct file
{
	u16string filename;
	bool read_access;
	bool write_access;
	filestatus status;
	uint64 position;
	uint64 size;
	void *handler;

	file();
	~file();
	/*Return if path contained in filename exists*/
	bool exists();
	/*Try to open file with read_access and write_access attributes
	status identifies success of operation
	If succeded position resets to 0, size equals file size and handler contains OS handler*/
	void open();
	/*Close the file and reset status to filestatus::closed*/
	void close();
	/*Resize the file*/
	void resize(uint64 file_size);
	/*Read file bytes_size bytes starting from position to addr pointer
	position changes dependent on actual bytes read
	addr must be valid array pointer with valid size*/
	uint64 read(uint64 bytes_size, void *addr);
	/*Write bytes_size from addr pointer starting from position
	position changes dependent on actual bytes written
	addr must be valid array pointer with valid size*/
	uint64 write(void *addr, uint64 bytes_size);
	/*Remove file from filesystem*/
	bool remove();
	/*Move/rename file*/
	bool move(u16string &new_path);
};
