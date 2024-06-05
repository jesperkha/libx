// Include implementation with:
// #define LIBX_IMPL

#pragma once

#include <stdbool.h>

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define u64 unsigned long long
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
    ERR_LIST_FULL,                 // List capacity reached
    ERR_ITERATION_FINISH,          // Iterator is finished
} error;

typedef struct String
{
    char *str;
    int length;
    error err;
} String;

#define Str(s) \
    (String) { .err = ERR_NO_ERROR, .length = strlen(s), .str = s }

typedef struct StringIter
{
    String s;
    int pos;
    error err;
} StringIter;

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
#define Ok(o) ((o).err == 0)

// Arena

// Allocates new arena with given size. Sets err value on failure.
Arena ArenaNew(int size);
// Allocates a new temporary arena within the parent.
Arena ArenaTemp(Arena *parent, int size);
// Releases temporary arena. Fails if allocations to the parent were made after the temp was created.
error ArenaReleaseTemp(Arena *temp, Arena *parent);
// Returns NULL on failure and sets err value.
void *ArenaAlloc(Arena *a, int size);
// Frees internal memory pointer. Sets ERR_MEMORY_FREED.
error ArenaFree(Arena *a);

// IO utils

// Reads file and returns it. Sets error in File.err on failure.
// Uses default allocator, remember to call XFreeFile().
File XReadFile(const char *filepath);
// Use only when not allocated with Arena.
error XFreeFile(File f);

// Strings

// Note that all of these functions will short circuit/return default/error
// values if the string passed has an error. Therefore it is safe to chain
// multiple calls even with faulty strings.

// Allocates string in arena
String StrAlloc(Arena *a, const char *s);
// Allocates a copy of the string s
String StrCopy(Arena *a, String s);
// Returns character at pos in string. Panics if out of bounds.
char CharAt(String s, int pos);
// Returns the number of times c appears in s
int StrCount(String s, char c);
// Returns new string converted to upper case
String StrUpper(Arena *a, String s);
// Returns new string converted to lower case
String StrLower(Arena *a, String s);
// Allocates and returns new concatinated string.
String StrConcat(Arena *a, String s1, String s2);
// Returns index of first occurence of c. -1 if not found.
int StrFind(String s, char c);
// Returns true if strings are identical
bool StrCompare(String a, String b);

// String iterators

// Returns iterator
StringIter StrToIterator(String s);
// Returns string before next occurance of the delimeter. Can give empty string.
String StrSplit(StringIter *iter, char delim);

// List

// Returns pointer to new list
void *ListCreate(size_t dataSize, size_t length);
// Free list and header
void ListFree(void *list);
// Returns length of list. Only valid if appended to with ListAppend or append.
int ListLen(void *list);
// Return capacity of list, given in declaration.
int ListCap(void *list);
// Appends item to end of list. Fails if full.
void ListAppend(void *list, u64 item);
// Removes and returns last element in list.
void *ListPop(void *list);

#define List(T) T * // List type macro
#define list(T, size) (T *)ListCreate(sizeof(T), size)
#define append(list, item) ListAppend(list, (u64)item)
#define len(list) (ListLen(list))
#define cap(list) (ListCap(list))
#define pop(list) (ListPop(list))

//////////////////////////////////////////////////////////////////

// Implementation, include once by defining LIBX_IMPL before #include

#ifdef LIBX_IMPL

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#define xdefaultAlloc(size) (HeapAlloc(GetProcessHeap(), 0, size))
#define xdefaultFree(p) (HeapFree(GetProcessHeap(), 0, p))

typedef struct ListHeader
{
    int length;
    int cap;
    int dataSize;
    error err;
} ListHeader;

#define HEADER_SIZE (sizeof(ListHeader))

static char *error_msgs[] = {
    [ERR_NO_ERROR] = "No error",
    [ERR_FILE_READ] = "Failed to read from file",
    [ERR_NO_MEMORY] = "Out of memory",
    [ERR_DOUBLE_FREE] = "Multiple frees",
    [ERR_ARENA_RELEASE_AFTER_ALLOC] = "Temporary arena was freed after parent allocations",
    [ERR_TEMP_ARENA_FREE] = "Cannot free temporary arena",
    [ERR_LIST_FULL] = "List surpassed capacity",
    [ERR_ITERATION_FINISH] = "Iterator is empty",
};

char *XError(error e)
{
    return error_msgs[e];
}

Arena ArenaNew(int size)
{
    void *p = xdefaultAlloc(size);
    if (p == NULL)
        return (Arena){.err = ERR_NO_MEMORY};

    return (Arena){
        .err = ERR_NO_ERROR,
        .memory = p,
        .pos = 0,
        .size = size,
        .depth = 0,
    };
}

void *ArenaAlloc(Arena *a, int size)
{
    if (!Ok(*a))
        return NULL;

    if (a->pos + size > a->size)
    {
        a->err = ERR_NO_MEMORY;
        return NULL;
    }

    void *p = a->memory + a->pos;
    a->pos += size;
    return p;
}

Arena ArenaTemp(Arena *parent, int size)
{
    if (!Ok(*parent))
        (Arena){.err = parent->err};

    void *p = ArenaAlloc(parent, size);
    if (p == NULL)
        return (Arena){.err = parent->err};

    return (Arena){
        .err = ERR_NO_ERROR,
        .memory = p,
        .pos = 0,
        .size = size,
        .depth = parent->depth + 1,
    };
}

error ArenaReleaseTemp(Arena *temp, Arena *parent)
{
    if (!Ok(*temp))
        return temp->err;
    if (!Ok(*parent))
        return parent->err;

    // If allocations have been made after creating the temp Arena
    if (parent->memory + parent->pos - temp->size != temp->memory)
        return ERR_ARENA_RELEASE_AFTER_ALLOC;

    parent->pos -= temp->size;
    temp->err = ERR_MEMORY_FREED;
    return ERR_NO_ERROR;
}

error ArenaFree(Arena *a)
{
    if (a->err == ERR_MEMORY_FREED)
        return ERR_DOUBLE_FREE;
    if (!Ok(*a))
        return a->err;
    if (a->depth != 0)
        return ERR_TEMP_ARENA_FREE;

    xdefaultFree(a->memory);
    a->err = ERR_MEMORY_FREED;
    return ERR_NO_ERROR;
}

File XReadFile(const char *filepath)
{
    File f = {0};

    HANDLE file = CreateFileA(filepath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        goto return_error;

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

static ListHeader *getHeader(void *ptr)
{
    return (ListHeader *)(ptr - HEADER_SIZE);
}

void *ListCreate(size_t dataSize, size_t length)
{
    ListHeader header = {
        .length = 0,
        .cap = length,
        .dataSize = dataSize,
    };

    void *listptr = xdefaultAlloc(HEADER_SIZE + (dataSize * length));
    memcpy(listptr, &header, HEADER_SIZE);
    return (listptr + HEADER_SIZE);
}

int ListLen(void *list)
{
    return getHeader(list)->length;
}

int ListCap(void *list)
{
    return getHeader(list)->cap;
}

void ListAppend(void *list, u64 item)
{
    ListHeader *header = getHeader(list);
    int size = header->dataSize;

    if (header->length < header->cap)
    {
        int length = (header->length * size);

        if (size > sizeof(u64))
            memcpy(list + length, (void *)item, size);
        else
            memcpy(list + length, &item, size);

        header->length++;
    }
}

void *ListPop(void *list)
{
    ListHeader *header = getHeader(list);
    header->length--;
    return list + (header->length * header->dataSize);
}

void ListFree(void *list)
{
    free(list - HEADER_SIZE);
}

#define returnIfError(s) \
    if (!Ok(s))          \
        return (String) { .err = s.err, .length = 0 }

String StrAlloc(Arena *a, const char *s)
{
    int length = strlen(s);
    char *p = ArenaAlloc(a, length);
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

String StrCopy(Arena *a, String s)
{
    returnIfError(s);

    char *str = ArenaAlloc(a, s.length);
    if (str == NULL)
        return (String){.err = ERR_NO_MEMORY};

    memcpy(str, s.str, s.length);
    return (String){.err = ERR_NO_ERROR, .length = s.length, .str = str};
}

char CharAt(String s, int pos)
{
    if (s.length <= pos)
    {
        printf("Panic: string index out of bounds\n");
        exit(1);
    }
    if (!Ok(s))
    {
        printf("Panic: CharAt on string with error\n");
        exit(1);
    }

    return *(s.str + pos);
}

int StrCount(String s, char c)
{
    if (!Ok(s))
        return 0;

    int count = 0;
    for (int i = 0; i < s.length; i++)
    {
        if (CharAt(s, i) == c)
            count++;
    }
    return count;
}

String StrUpper(Arena *a, String s)
{
    returnIfError(s);

    String upper = StrCopy(a, s);
    returnIfError(upper);

    for (int i = 0; i < s.length; i++)
    {
        char c = CharAt(s, i);
        if (c >= 'a' && c <= 'z')
            c -= 32;
        *(upper.str + i) = c;
    }

    return upper;
}

String StrLower(Arena *a, String s)
{
    returnIfError(s);

    String lower = StrCopy(a, s);
    returnIfError(lower);

    for (int i = 0; i < s.length; i++)
    {
        char c = CharAt(s, i);
        if (c >= 'A' && c <= 'Z')
            c += 32;
        *(lower.str + i) = c;
    }

    return lower;
}

StringIter StrToIterator(String s)
{
    return (StringIter){
        .err = s.err,
        .pos = 0,
        .s = s,
    };
}

String StrSplit(StringIter *iter, char delim)
{
    if (!Ok(*iter))
        return (String){.err = iter->err};

    int start = iter->pos;
    while (iter->pos < iter->s.length)
    {
        char c = CharAt(iter->s, iter->pos);
        if (c == delim)
        {
            char *str = iter->s.str + start;
            int length = iter->pos - start;
            iter->pos += 1;

            return (String){
                .length = length,
                .str = str,
                .err = ERR_NO_ERROR,
            };
        }

        iter->pos++;
    }

    iter->err = ERR_ITERATION_FINISH;
    return (String){
        .length = iter->pos - start,
        .str = iter->s.str + start,
        .err = ERR_NO_ERROR,
    };
}

int StrFind(String s, char c)
{
    if (!Ok(s))
        return -1;
    for (int i = 0; i < s.length; i++)
        if (CharAt(s, i) == c)
            return i;
    return -1;
}

String StrConcat(Arena *a, String s1, String s2)
{
    returnIfError(s1);
    returnIfError(s2);

    int length = s1.length + s2.length;
    char *str = ArenaAlloc(a, length);
    memcpy(str, s1.str, s1.length);
    memcpy(str + s1.length, s2.str, s2.length);
    return (String){
        .err = ERR_NO_ERROR,
        .str = str,
        .length = length,
    };
}

bool StrCompare(String a, String b)
{
    if (!Ok(a) || !Ok(b))
        return false;
    if (a.length != b.length)
        return false;
    for (int i = 0; i < a.length; i++)
        if (CharAt(a, i) != CharAt(b, i))
            return false;
    return true;
}

#endif