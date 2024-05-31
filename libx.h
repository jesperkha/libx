#pragma once

#include <stdbool.h>
#include <string.h>

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define u64 unsigned long
#define i8 char
#define i16 short
#define i32 int
#define i64 long
#define f32 float

// Defer a function call or expression to be evaluated after the preceding block.
#define defer(f) for (int __defer_i = 0; !__defer_i; (f, __defer_i++))

// Error code. Get message with XError().
typedef enum error
{
    NO_ERROR,
    ERR_FILE_READ,
    ERR_NO_MEMORY,
} error;

typedef struct String
{
    char *str;
    int length;
    error err;
} String;

// Makes a new string
#define str(s) ((String){.str = s, .length = strlen(s), .err = 0})

typedef struct Arena
{
    void *memory;
    int size;
    int pos;
    bool freed;
    error err;
} Arena;

typedef struct File
{
    char filepath[260];
    int size;
    char *data;
    error err;
} File;

// Returns string representation of error code.
char *XError(error e);

// Can only be used with object containing an 'err' field.
#define Ok(o) (o.err == 0)

Arena XArenaNew(int size);             // Allocates new arena with given size. Sets Arena.err on failure.
void *XArenaAlloc(Arena *a, int size); // Returns NULL on failure and sets Arena.err.
void XArenaFree(Arena a);              // Frees internal memory pointer.

// Reads file and returns it. Sets error in File.err on failure.
// Uses default allocator, remember to call XFreeFile().
File XReadFile(const char *filepath);
void XFreeFile(File f); // Use only when not allocated with Arena
