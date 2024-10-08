#pragma once

#include <stdbool.h>
#include <windows.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef float f32;

#define defer(f) for (int __defer_i = 0; !__defer_i; (f, __defer_i++))

typedef enum error
{
    ERR_NO_ERROR = 0,
    ERR_FILE_READ,
    ERR_NO_MEMORY,
    ERR_MEMORY_FREED,
    ERR_DOUBLE_FREE,
    ERR_ARENA_RELEASE_AFTER_ALLOC,
    ERR_TEMP_ARENA_FREE,
    ERR_LIST_FULL,
    ERR_ITERATION_FINISH,
    ERR_FILE_NOT_FOUND,
    ERR_NULL_PTR,
} error;

typedef struct String
{
    char *str;
    u32 length;
    error err;
} String;

#define STRING(s) \
    (String) { .err = 0, .length = strlen((s)), .str = (s) }

typedef struct StringIter
{
    String s;
    u32 pos;
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
    u64 size;
    char *data; // Always check NULL, .open, and .err
    error err;

    bool open;  // If true, file contains a data field with its contents
    bool isDir; // True if the filepath corresponds with a directory
    bool readOnly;
} File;

typedef struct FileIter
{
    WIN32_FIND_DATA data;
    HANDLE hFind;
    error err;
} FileIter;

// Returns string representation of error code.
char *XError(error e);

// Prints message and exits with status 1
void panic(char *msg);

// Can only be used with object containing an 'err' field.
#define Ok(o) ((o).err == 0)

#define assertOk(o) \
    if (!Ok(o))     \
        panic(XError((o).err));

//: doc_begin

// Allocates new arena with given size. Sets err value on failure.
Arena ArenaNew(u64 size);
// Allocates a new temporary arena within the parent.
Arena ArenaTemp(Arena *parent, u64 size);
// Releases temporary arena. Fails if allocations to the parent were made after the temp was created.
error ArenaReleaseTemp(Arena *temp, Arena *parent);
// Returns NULL on failure and sets err value.
void *ArenaAlloc(Arena *a, u64 size);
// Frees internal memory pointer. Sets ERR_MEMORY_FREED.
error ArenaFree(Arena *a);

// Reads file and returns it. Sets error in File.err on failure.
// Uses default allocator, remember to call XFreeFile().
File XReadFile(const char *filepath);
// Use only when not allocated with Arena.
error XFreeFile(File *f);
// Returns file iterator for given directory
FileIter XReadDir(const char *path);
// Returns next file if any. Closes iterator when done.
File FileIterNext(FileIter *iter);
// Closes file iterator. Only necessary if iteration is stopped early.
error XCloseFileIter(FileIter *iter);

// Note that all of these functions will short circuit/return default/error
// values if the string passed has an error. Therefore it is safe to chain
// multiple calls even with faulty strings.

void prints(String s);
// Allocates string in arena
String StrAlloc(Arena *a, const char *s);
// Allocates a copy of the string s
String StrCopy(Arena *a, String s);
// Returns substring of s from start to end
String StrSub(String s, u32 start, u32 end);
// Returns character at pos in string. Panics if out of bounds.
char CharAt(String s, u32 pos);
// Returns the number of times c appears in s
u32 StrCount(String s, char c);
// Returns new string converted to upper case
String StrUpper(Arena *a, String s);
// Returns new string converted to lower case
String StrLower(Arena *a, String s);
// Allocates and returns new concatinated string.
String StrConcat(Arena *a, String s1, String s2);
// Returns index of first occurence of c. -1 if not found.
u32 StrFind(String s, char c);
// Returns index of first occurence of word, -1 if not found.
u32 StrFindWord(String s, char *word);
// Returns index of first occurence of word, -1 if not found.
u32 StrFindString(String s, String word);
// Returns true if strings are identical
bool StrCompare(String a, String b);

StringIter StrToIterator(String s);
StringIter StrToIteratorEx(char *string, u32 length);
// Returns string before next occurance of the delimeter. Can give empty string.
String StrSplit(StringIter *iter, char delim);

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

//: doc_end

#define List(T) T * // List type macro
#define list(T, size) (T *)ListCreate(sizeof(T), size)
#define append(list, item) ListAppend(list, (u64)item)
#define len(list) (ListLen(list))
#define cap(list) (ListCap(list))
#define pop(list) (ListPop(list))

//////////////////////////////////////////////////////////////////

// Implementation, include once by defining LIBX before #include

#ifdef LIBX

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define xdefaultAlloc(size) (HeapAlloc(GetProcessHeap(), 0, size))
#define xdefaultFree(p) (HeapFree(GetProcessHeap(), 0, p))

void panic(char *msg)
{
    printf("Panic: %s\n", msg);
    exit(1);
}

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
    [ERR_MEMORY_FREED] = "Memory marked as free",
    [ERR_DOUBLE_FREE] = "Multiple frees",
    [ERR_ARENA_RELEASE_AFTER_ALLOC] = "Temporary arena was freed after parent allocations",
    [ERR_TEMP_ARENA_FREE] = "Cannot free temporary arena",
    [ERR_LIST_FULL] = "List surpassed capacity",
    [ERR_ITERATION_FINISH] = "Iterator is empty",
    [ERR_FILE_NOT_FOUND] = "File not found",
    [ERR_NULL_PTR] = "NULL pointer exception",
};

char *XError(error e)
{
    if (e > sizeof(error_msgs))
        panic("error out of bounds\n");

    return error_msgs[e];
}

Arena ArenaNew(u64 size)
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

void *ArenaAlloc(Arena *a, u64 size)
{
    if (a == NULL)
        return NULL;
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

Arena ArenaTemp(Arena *parent, u64 size)
{
    if (parent == NULL)
        return (Arena){.err = ERR_NULL_PTR};
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
    if (temp == NULL || parent == NULL)
        return ERR_NULL_PTR;
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
    if (a == NULL)
        return ERR_NULL_PTR;
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
    if (filepath == NULL)
        return (File){.err = ERR_NULL_PTR};

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
    f.open = true;
    strncpy(f.filepath, filepath, 260);
    return f;

return_error:
    f.err = ERR_FILE_READ;
    return f;
}

error XFreeFile(File *f)
{
    if (f == NULL)
        return ERR_NULL_PTR;
    if (f->err == ERR_MEMORY_FREED)
        return ERR_DOUBLE_FREE;
    if (!Ok(*f))
        return f->err;

    xdefaultFree(f->data);
    f->err = ERR_MEMORY_FREED;
    f->open = false;
    return ERR_NO_ERROR;
}

FileIter XReadDir(const char *path)
{
    if (path == NULL)
        return (FileIter){.err = ERR_NULL_PTR};

    FileIter iter = {0};

    // FindFirstFile expects a * wildcard at end
    char buf[512];
    strcpy(buf, path);
    strcat(buf, "\\*");

    HANDLE hFind = FindFirstFileA(buf, &iter.data);
    if (hFind == INVALID_HANDLE_VALUE)
        return (FileIter){.err = ERR_FILE_NOT_FOUND};

    iter.hFind = hFind;
    return iter;
}

File FileIterNext(FileIter *iter)
{
    if (iter == NULL)
        return (File){.err = ERR_NULL_PTR};
    if (!Ok(*iter))
        return (File){.err = iter->err};

    File f = {0};
    strcpy(f.filepath, iter->data.cFileName);
    f.size = (u64)iter->data.nFileSizeLow | ((u64)iter->data.nFileSizeHigh << 32);
    f.isDir = iter->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    f.readOnly = iter->data.dwFileAttributes & FILE_ATTRIBUTE_READONLY;

    if (!FindNextFileA(iter->hFind, &iter->data))
    {
        iter->err = ERR_ITERATION_FINISH;
        FindClose(iter->hFind);
    }

    return f;
}

error XCloseFileIter(FileIter *iter)
{
    if (iter == NULL)
        return ERR_NULL_PTR;
    if (!Ok(*iter))
        return iter->err;
    FindClose(iter->hFind);
    iter->err = ERR_ITERATION_FINISH;
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
        return (String) { .err = (s).err, .length = 0 }

String StrAlloc(Arena *a, const char *s)
{
    if (s == NULL || a == NULL)
        return (String){.err = ERR_NULL_PTR};
    returnIfError(*a);

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

void prints(String s)
{
    if (s.err)
        printf("STRING_ERROR\n");
    printf("%.*s\n", s.length, s.str);
}

String StrSub(String s, u32 start, u32 end)
{
    return (String){
        .str = s.str + start,
        .length = end - start,
        .err = s.err,
    };
}

u32 _strFindWordEx(String s, char *word, int wordlen)
{
    for (int i = 0; i < s.length; i++)
    {
        if (s.str[i] == word[0])
        {
            bool found = true;
            for (int j = 0; j < wordlen && i + j < s.length; j++)
                if (s.str[i + j] != word[j])
                    found = false;

            if (found)
                return i;
        }
    }
    return -1;
}

u32 StrFindWord(String s, char *word)
{
    return _strFindWordEx(s, word, strlen(word));
}

u32 StrFindString(String s, String word)
{
    return _strFindWordEx(s, word.str, word.length);
}

String StrCopy(Arena *a, String s)
{
    if (a == NULL)
        return (String){.err = ERR_NULL_PTR};
    returnIfError(s);

    char *str = ArenaAlloc(a, s.length);
    if (str == NULL)
        return (String){.err = ERR_NO_MEMORY};

    memcpy(str, s.str, s.length);
    return (String){.err = ERR_NO_ERROR, .length = s.length, .str = str};
}

char CharAt(String s, u32 pos)
{
    if (!Ok(s))
        panic("CharAt on string with error\n");
    if (s.length <= pos)
        panic("string index out of bounds\n");

    return *(s.str + pos);
}

u32 StrCount(String s, char c)
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
    if (a == NULL)
        return (String){.err = ERR_NULL_PTR};
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
    if (a == NULL)
        return (String){.err = ERR_NULL_PTR};
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

StringIter StrToIteratorEx(char *string, u32 length)
{
    return (StringIter){
        .err = 0,
        .pos = 0,
        .s = (String){
            .err = 0,
            .str = string,
            .length = length,
        },
    };
}

String StrSplit(StringIter *iter, char delim)
{
    if (iter == NULL)
        return (String){.err = ERR_NULL_PTR};
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

u32 StrFind(String s, char c)
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
    if (a == NULL)
        return (String){.err = ERR_NULL_PTR};
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