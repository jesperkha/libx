#include "libx.h"
#include <windows.h>
#include <stdlib.h>

#define xdefaultAlloc(size) (malloc(size))
#define xdefaultFree(p) (free(p))

static char *error_msgs[] = {
	[NO_ERROR] = "No error",
	[ERR_FILE_READ] = "Failed to read from file",
	[ERR_NO_MEMORY] = "Out of memory",
};

char *XError(error e)
{
	return error_msgs[e];
}

Arena XArenaNew(int size)
{
	void *m = xdefaultAlloc(size);
	if (m == NULL)
		return (Arena){.err = ERR_NO_MEMORY};

	return (Arena){
		.err = NO_ERROR,
		.freed = false,
		.memory = m,
		.pos = 0,
		.size = size,
	};
}

void *XArenaAlloc(Arena *a, int size)
{
	if (a->pos + size >= a->size)
	{
		a->err = ERR_NO_MEMORY;
		return NULL;
	}

	void *p = a->memory + a->pos;
	a->pos += size;
	return p;
}

void XArenaFree(Arena a)
{
	xdefaultFree(a.memory);
}

File XReadFile(const char *filepath)
{
	File f = {0};

	// Open file. EditorOpenFile does not create files and fails on file-not-found
	HANDLE file = CreateFileA(filepath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		goto return_error;

	// Get file size and read file contents into string buffer
	DWORD bufSize = GetFileSize(file, NULL) + 1;
	DWORD read;
	char *buffer = xdefaultAlloc(bufSize);
	if (!ReadFile(file, buffer, bufSize, &read, NULL))
	{
		CloseHandle(file);
		goto return_error;
	}

	CloseHandle(file);
	buffer[bufSize - 1] = 0;
	f.size = bufSize - 1;
	f.data = buffer;
	strncpy(f.filepath, filepath, 260);
	return f;

return_error:
	f.err = ERR_FILE_READ;
	return f;
}

void XFreeFile(File f)
{
	xdefaultFree(f.data);
}