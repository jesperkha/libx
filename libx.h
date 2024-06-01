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

// Defer a function call or expression to be evaluated after the preceding block.
#define defer(f) for (int __defer_i = 0; !__defer_i; (f, __defer_i++))

// Error code. Get message with XError().
typedef enum error
{
    ERR_NO_ERROR,                  // No error
    ERR_FILE_READ,                 // Failed to read file
    ERR_NO_MEMORY,                 // Ran out of memory
    ERR_MEMORY_FREED,              // Marked as freed memory
    ERR_DOUBLE_FREE,               // Memory freed more than once
    ERR_ARENA_RELEASE_AFTER_ALLOC, // Temp arena released after parent alloc
    ERR_TEMP_ARENA_FREE,           // Tried to free a temp arena
} error;

typedef struct String
{
    char *str;
    int length;
    error err;
} String;

typedef struct Arena
{
    void *memory;
    int size;
    int pos;
    int depth; // 0 for base arena
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

// Allocates new arena with given size. Sets err value on failure.
Arena XArenaNew(int size);
// Allocates a new temporary arena within the parent.
Arena XArenaTemp(Arena *parent, int size);
// Releases temporary arena. Fails if allocations to the parent were made after the temp was created.
error XArenaReleaseTemp(Arena *temp, Arena *parent);
// Returns NULL on failure and sets err value.
void *XArenaAlloc(Arena *a, int size);
// Frees internal memory pointer. Sets ERR_MEMORY_FREED.
error XArenaFree(Arena a);

// Reads file and returns it. Sets error in File.err on failure.
// Uses default allocator, remember to call XFreeFile().
File XReadFile(const char *filepath);
// Use only when not allocated with Arena.
error XFreeFile(File f);

// Returns string object from literal
String XStr(char *s);
// Allocates string in arena
String XAllocStr(Arena *a, const char *s);