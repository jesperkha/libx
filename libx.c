#include "libx.h"
#include <windows.h>
#include <stdlib.h>

static char *error_msgs[] = {
	[NO_ERROR] = "No error",
	[ERR_FILE_READ] = "Failed to read from file",
};

string XError(error e)
{
	return error_msgs[e];
}

error XReadFile(const char *filepath, File *f)
{
	// Open file. EditorOpenFile does not create files and fails on file-not-found
	HANDLE file = CreateFileA(filepath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return ERR_FILE_READ;

	// Get file size and read file contents into string buffer
	DWORD bufSize = GetFileSize(file, NULL) + 1;
	DWORD read;
	char *buffer = malloc(bufSize);
	if (!ReadFile(file, buffer, bufSize, &read, NULL))
	{
		CloseHandle(file);
		return ERR_FILE_READ;
	}

	CloseHandle(file);
	buffer[bufSize - 1] = 0;
	f->size = bufSize - 1;
	f->data = buffer;
	return NO_ERROR;
}

void XFreeFile(File f)
{
	free(f.data);
}