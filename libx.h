#pragma once

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define u64 unsigned long
#define i8 char
#define i16 short
#define i32 int
#define i64 long
#define f32 float

typedef char *string; // A string can be assumed to be null-terminated

// Error code. Get message with XError().
typedef enum error
{
    NO_ERROR,
    ERR_FILE_READ,
} error;

// Returns string representation of error code.
string XError(error e);

typedef struct File
{
    char filepath[260];
    int size;
    char *data;
} File;

// Reads file and writes to dest. Remember to call XFreeFile().
error XReadFile(const char *filepath, File *dest);

// Frees file data pointer, not file object if allocated.
void XFreeFile(File f);