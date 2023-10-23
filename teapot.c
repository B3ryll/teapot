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

    habit val =
    {
        .name   = "Ler mangas",
        .type   = HABIT_TYPE_NUMBER,

        .metric       = "páginas lidas",
        .metric_short = "pag",
    };

    store_habit (st, val, alloc);

    return 0;
}
