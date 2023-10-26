// ======================================================
//  ./store.h
// 
//  handle habit data persistence in local file-based
//  data store.
//
// ======================================================

#include <stdlib.h>

#ifndef _STORE_H
#define _STORE_H

// --------------------------------------------------------- //
//  base definitions

enum
{
    HABIT_TYPE_NUMBER,
    HABIT_TYPE_BOOLEAN,
};

struct habit
{
    short int id;

    char* name;
    char  type;

    char* metric;
    char* metric_short;
};
typedef struct habit habit;

struct habit_entry
{
    char* day;
    char* habit_id;

    int   value;
};
typedef struct habit_entry entry;

struct store
{
    const char* dirpath;
};
typedef struct store store;

// --------------------------------------------------------- //
//  util definitions

struct allocator 
{
    void* (*allocate)(size_t len);
    int   (*free)(void* mem);
};
typedef struct allocator allocator;

// --------------------------------------------------------- //
//   habit persistence api

int store_init (store st, allocator alloc);

int store_habit  (store st, habit val, allocator alloc);
int update_habit (store st, habit val, allocator alloc);
int remove_habit (store st, short int id, allocator alloc);

habit* query_habit (store st, habit val, allocator alloc);

// --------------------------------------------------------- //
//  habit entries persistence api

// @ TODO

#endif
