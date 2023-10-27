#include "store.h"

#include <stdio.h>
#include <string.h>

void* allocate_func (size_t len)
{
   return malloc(len);
}

int  free_func (void* val)
{
    free(val);
    return 0;
}

int main (int argc, char* argv[])
{
    allocator alloc = {
        .allocate = allocate_func,
        .free     = free_func,
    }; 

    store st = {
        .dirpath = "./test/.res", 
    };

    habit query = {.id = 9};
    habit* res  = query_habit (st, query, alloc);

    fprintf (stderr, "name of the habit => %s\n", res->metric);

    return 0;
}
