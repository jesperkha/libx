#include "libx.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#define xdefaultAlloc(size) (HeapAlloc(GetProcessHeap(), 0, size))
#define xdefaultFree(p) (HeapFree(GetProcessHeap(), 0, p))

static char *error_msgs[] = {
	[ERR_NO_ERROR] = "No error",
	[ERR_FILE_READ] = "Failed to read from file",
	[ERR_NO_MEMORY] = "Out of memory",
	[ERR_DOUBLE_FREE] = "Multiple frees",
	[ERR_ARENA_RELEASE_AFTER_ALLOC] = "Temporary arena was freed after parent allocations",
	[ERR_TEMP_ARENA_FREE] = "Cannot free temporary arena",
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
		.err = ERR_NO_ERROR,
		.memory = m,
		.pos = 0,
		.size = size,
	};
}

void *XArenaAlloc(Arena *a, int size)
{
	if (a->pos + size > a->size)
	{
		a->err = ERR_NO_MEMORY;
		return NULL;
	}

	void *p = a->memory + a->pos;
	a->pos += size;
	return p;
}

Arena XArenaTemp(Arena *parent, int size)
{
	void *p = XArenaAlloc(parent, size);
	if (p == NULL)
		return (Arena){.err = parent->err};

	return (Arena){
		.err = ERR_NO_ERROR,
		.memory = p,
		.pos = 0,
		.size = size,
	};
}

error XArenaReleaseTemp(Arena *temp, Arena *parent)
{
	// If allocations have been made after creating the temp Arena
	if (parent->memory + parent->pos - temp->size != temp->memory)
		return ERR_ARENA_RELEASE_AFTER_ALLOC;

	parent->pos -= temp->size;
	temp->err = ERR_MEMORY_FREED;
	return ERR_NO_ERROR;
}

error XArenaFree(Arena a)
{
	if (a.err == ERR_MEMORY_FREED)
		return ERR_DOUBLE_FREE;

	if (a.depth != 0)
		return ERR_TEMP_ARENA_FREE;

	xdefaultFree(a.memory);
	a.err = ERR_MEMORY_FREED;
	return ERR_NO_ERROR;
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

error XFreeFile(File f)
{
	if (f.err == ERR_MEMORY_FREED)
		return ERR_DOUBLE_FREE;

	xdefaultFree(f.data);
	f.err = ERR_MEMORY_FREED;
	return ERR_NO_ERROR;
}

String XStr(char *s)
{
	return (String){
		.err = ERR_NO_ERROR,
		.length = strlen(s),
		.str = s,
	};
}

String XAllocStr(Arena *a, const char *s)
{
	int length = strlen(s);
	char *p = (char *)XArenaAlloc(a, length);
	if (p == NULL)
		return (String){
			.err = ERR_NO_MEMORY,
			.length = 0,
			.str = NULL,
		};

	memcpy(p, s, length);
	return (String){
		.err = ERR_NO_ERROR,
		.length = length,
		.str = p,
	};
}