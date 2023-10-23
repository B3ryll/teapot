// ==========================================================
//  ./store.c
//
// ==========================================================

#include "store.h"

#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "csv.h"

// --------------------------------------------------------- //

#define BUFF_SIZE 1024

#define ID_STR_LEN   4
#define TYPE_STR_LEN 1

// --------------------------------------------------------- //

static int last_habit_id (store st, allocator alloc);

char* habit_serialize (habit, val, allocator alloc);

static int is_space (unsigned char c);
static int is_term  (unsigned char c);

static inline char* concat_path (
    char* path1,
    char* path2,
    allocator alloc
);

static void habit_id_field_callback (
    void* data,
    size_t len,
    void* out
);
static void habit_id_line_callback  (
    int end,
    void *out
);
static int last_habit_id (store id, allocator alloc);

// --------------------------------------------------------- //

char* habit_serialize (habit val, allocator alloc)
{
    short int out_str_len =
        strlen(val.name)
        + ID_STR_LEN
        + TYPE_STR_LEN
        + strlen(val.metric)
        + strlen(val.metric_short)
        + 11; 
    // 5 = (',' x 4) + ('\"' x 6) + '\0'
      
    char* serial = (*alloc.allocate)(out_str_len);
    sprintf(
        serial, "%04x,\"%s\",%d,\"%s\",\"%s\"",
        val.id,
        val.name,
        val.type,
        val.metric,
        val.metric_short
    );

    return serial;
}

// --------------------------------------------------------- //

static int is_space (unsigned char c)
{
    if (c == CSV_SPACE || c == CSV_TAB)
        return 1;

    return 0;
}

static int is_term (unsigned char c)
{
    if (c == CSV_CR || c == CSV_LF)
        return 1;

    return 0;
}

// --------------------------------------------------------- //

static inline char* concat_path (
    char* path1,
    char* path2, 
    allocator alloc
) {
    char* fullpath = (*alloc.allocate)(
       strlen(path1) + strlen(path2) + 2
    );
    
    sprintf(fullpath, "%s/%s", path1, path2);
    return fullpath;
}

int store_init (store st, allocator alloc)
{
    struct stat store_stat = {0};
    
    fprintf (
        stderr, "(store path => %s)\n",
        st.dirpath
    );
    if ((stat(st.dirpath, &store_stat)) != -1)
    {
        mkdir(st.dirpath, 0700);
    }

    // @note : the largest string must come first
    char* records[] = {
        "daily_entry_records.csv",
        "entry_records.csv",
        "last_report_date",
        "habit_data.csv",
    };

    char* habit_data_path = concat_path (
       (char*) st.dirpath, records[0], alloc
    );
    const short int alloc_path_size =
        strlen(st.dirpath) + strlen(records[0]) + 2;

    const short int dir_path_len = strlen(st.dirpath);

    // @ TODO : refactor this code later
    for (int count = 0; count < 4; count++)
    {
        if (count)
            sprintf(
                habit_data_path + dir_path_len + 1,
                "%s", records[count]
            );

        if ((stat(habit_data_path, &store_stat) == -1))
        {
            FILE* file;

            file = fopen (habit_data_path, "wb");
            if (file == NULL)
            {
                fprintf (
                    stderr, "[failed to open file '%s']",
                    habit_data_path
                );
            }

            fclose(file);
        }
    }

    (*alloc.free)(habit_data_path);

    return 0;
}

// --------------------------------------------------------- //

typedef struct
{
    int  latest_id;
    char is_first_field;

} id_query_ctx;

static void habit_id_field_callback (
    void* data,
    size_t len,
    void* out
) {
    id_query_ctx* ctx = out;

    if (!ctx->is_first_field)
    {
        return;
    }
    ctx->is_first_field = 0;

    int id = (int)strtol(data, NULL, 16);
    if (ctx->latest_id < id)
    {
        ctx->latest_id = id;
    }
}

static void habit_id_line_callback (int _end, void* out)
{
    ((id_query_ctx *) out)->is_first_field = 1;
}

//
// helper function which allows to search the last inserted
// habit id in the storage file
//
static int last_habit_id (store st, allocator alloc)
{
    struct csv_parser parser;

    unsigned char options = CSV_STRICT|CSV_APPEND_NULL;
    int err = csv_init(&parser, options);
    if (err != 0)
    {
        fprintf (stderr, "failed to initialize parser\n");
        exit (EXIT_FAILURE);
    }

    csv_set_space_func (&parser, is_space);
    csv_set_term_func  (&parser, is_term);

    char* habit_db_path = concat_path (
       (char*) st.dirpath, "habit_data.csv", alloc
    );
    if (habit_db_path == NULL)
    {
        fprintf (
            stderr, "failed to allocate path string",
            habit_db_path
        );

        exit (EXIT_FAILURE);
    }

    FILE* file = fopen(habit_db_path, "rb");
    if (file == NULL)
    {
        fprintf (
            stderr, "failed to open file %s\n",
            habit_db_path
        );

        (*alloc.free)(habit_db_path);

        exit (EXIT_FAILURE);
    }

    id_query_ctx ctx = {0, 1};

    char   buff [BUFF_SIZE];
    size_t read_bytes;

    while ((read_bytes = fread (buff, 1, BUFF_SIZE, file)) > 0)
    {
        int result = csv_parse (
            &parser,
            buff,
            read_bytes,
            habit_id_field_callback,
            habit_id_line_callback,
            &ctx
        );
        if (result != read_bytes)
        {
            fprintf (
                stderr, "failed to parse csv file: %s\n",
                habit_db_path
            );

            (*alloc.free)(habit_db_path);
            fclose(file);

            exit (EXIT_FAILURE);
        }
    }

    (*alloc.free)(habit_db_path);
    fclose(file);

    return ctx.latest_id;
}

// --------------------------------------------------------- //

int store_habit (store st, habit val, allocator alloc)
{
    val.id = last_habit_id (st, alloc) + 1;

    char* habit_db_path = concat_path (
       (char*) st.dirpath, "habit_data.csv", alloc
    );

    FILE* file   = fopen (habit_db_path, "ab");
    char* serial = habit_serialize (val, alloc);

    fprintf(file, "%s\n", serial);

    (*alloc.free)(serial);
    fclose(file);

    return 0;
}

// --------------------------------------------------------- //


