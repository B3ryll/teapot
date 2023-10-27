// ===========================================================
//  ./store.c
//
//  @TODO refactor habit store methods
//
// ===========================================================

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

struct habit_linked_list
{
    habit                     value;
    struct habit_linked_list* next;
};

static int last_habit_id (store st, allocator alloc);

char* habit_serialize (habit val, allocator alloc);

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

static inline void free_habit_linked_list_item (
    struct habit_linked_list* val,
    allocator alloc
) {
    (*alloc.free) (val->value.name);
    (*alloc.free) (val->value.metric);
    (*alloc.free) (val->value.metric_short);

    (*alloc.free) (val);
}

// @TODO refactor this code later
static inline void copy_habit (
    habit* out,
    habit source,
    allocator alloc
) {
    if (out->type != source.type)
    {
        out->type = source.type;
    }

    // ------------------------------------------- //
    if (strlen(out->name) < strlen(source.name))
    {
        (*alloc.free) (out->name);

        out->name = (*alloc.allocate) (
            strlen(source.name) + 1
        );

        strcpy(out->name, source.name);
    }
    else if (strcmp(out->name, source.name) != 0)
    {
        strcpy(out->name, source.name);
    }

    // ------------------------------------------- //
    if (strlen(out->metric) < strlen(source.metric))
    {
        (*alloc.free) (out->metric);

        out->metric = (*alloc.allocate) (
            strlen(source.metric) + 1
        );

        strcpy(out->metric, source.metric);
    }
    else if (strcmp(out->metric, source.metric) != 0)
    {
        strcpy(out->metric, source.metric);
    }

    // ------------------------------------------- //
    if (strlen(out->metric_short) < strlen(source.metric_short))
    {
        (*alloc.free) (out->metric_short);

        out->metric_short = (*alloc.allocate) (
            strlen(source.metric_short) + 1
        );

        strcpy(out->metric_short, source.metric_short);
    }
    else if (strcmp(out->metric_short, source.metric_short) != 0)
    {
        strcpy(out->metric_short, source.metric_short);
    }
}

static inline void free_habit_linked_list (
    struct habit_linked_list* list,
    allocator alloc
) {
    
    struct habit_linked_list* current = list;
    do
    {
        struct habit_linked_list* to_free = current;
        current                           = current->next;

        free_habit_linked_list_item (to_free, alloc);

    } while (current != NULL);
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
    char* fullpath = (*alloc.allocate) (
       strlen(path1) + strlen(path2) + 2
    );
    
    sprintf(fullpath, "%s/%s", path1, path2);
    return fullpath;
}

int store_init (store st, allocator alloc)
{
    struct stat store_stat = {0};
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

    csv_fini (
        &parser,
        habit_id_field_callback, 
        habit_id_line_callback,
        &ctx  
    );

    csv_free (&parser);
    (*alloc.free)(habit_db_path);

    err = fclose(file);
    if (err != 0)
    {
        // @TODO : handle error properly
    }

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

typedef struct
{
    struct habit_linked_list* list; 
    short int                 current_field;
    struct habit_linked_list* current;
    struct habit_linked_list* latest_allocated;
    allocator                 alloc;

} habit_filter_ctx;

static void habit_filter_field_callback (
    void*  data,
    size_t len,
    void*  out
) {
    habit_filter_ctx* ctx = out;

    switch (ctx->current_field++)
    {
    case 0:
        ctx->current->value.id = strtol (data, NULL, 16);
        break;

    case 1:
        ctx->current->value.name =
            (*ctx->alloc.allocate) (strlen (data) + 1);

        strcpy (ctx->current->value.name, data);
        break;

    case 2:
        ctx->current->value.type = strtol (data, NULL, 10);
        break;

    case 3:
        ctx->current->value.metric =
            (*ctx->alloc.allocate) (strlen (data) + 1);

        strcpy (ctx->current->value.metric, data);
        break;

    case 4:
        ctx->current->value.metric_short =
            (*ctx->alloc.allocate) (strlen (data) + 1);

        strcpy (ctx->current->value.metric_short, data);
        break;
    
    default:
        // @TODO: handle exception condition
        
        fprintf (
            stderr,
            "habit callback called with undefined field: %d",
            ctx->current_field - 1
        );
    }
}

static void habit_filter_line_callback (
    int   end,    
    void* out
) {
    habit_filter_ctx* ctx = out;

    if (ctx->current == NULL)
        return;

    if (ctx->list == NULL)
        return;

    struct habit_linked_list* item = (*ctx->alloc.allocate) (
        sizeof (struct habit_linked_list)
    );

    ctx->latest_allocated = ctx->current;

    ctx->current->next = item;
    ctx->current       = item;

    ctx->current_field = 0;
}

void persist_habit_list (
    char* filepath,
    struct habit_linked_list* list,
    allocator alloc
) {
    FILE* file = fopen (filepath, "wb");
    if (file == NULL)
    {
        fprintf (
            stderr, "failed to open file %s\n",
            filepath
        );

        exit (EXIT_FAILURE);
    }

    struct habit_linked_list* current = list;
    do
    {
        char* serial = habit_serialize (
            current->value, alloc
        );
        
        fprintf (file, "%s\n", serial);
        (*alloc.free) (serial);

        current = current->next;

    } while (current != NULL);
}

// @debug
void print_habit_linked_list (struct habit_linked_list list)
{
    struct habit_linked_list* current = &list; 
    do
    {
        fprintf (
            stderr, "name pointer => %x\n",
            current->value.name
        ); 
        fprintf (
            stderr, "name => %s\n",
            current->value.name
        ); 
        fprintf (
            stderr, "metric pointer => %x\n",
            current->value.metric
        ); 
        fprintf (
            stderr, "short metric pointer => %x\n",
            current->value.metric_short
        );

        current = current->next;
    } while (current != NULL);
}

struct habit_linked_list* delete_habit_from_list (
    struct habit_linked_list* list,
    short int id,
    allocator alloc   
) {
    struct habit_linked_list* previous = NULL;
    struct habit_linked_list* current  = list;

    struct habit_linked_list* first    = NULL;

    do
    {
        struct habit_linked_list* next = current->next;
        
        if (current->value.id == id)
        {
            free_habit_linked_list_item (current, alloc);
        }
        else
        {
            if (first == NULL)
                first = current;

            previous = current;
        }

        current = next;

        if (previous)
        {
            previous->next = next;
        }
    } while (current != NULL);

    return first;
}

int remove_habit (store st, short int id, allocator alloc)
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
    
    struct habit_linked_list* item =
        (*alloc.allocate) (sizeof (struct habit_linked_list));

    habit_filter_ctx ctx = {
        .list          = item,
        
        .current_field = 0,
        .current       = item,

        .alloc         = alloc,
    };
    
    char   buff [BUFF_SIZE];
    size_t read_bytes;
    
    while ((read_bytes = fread (buff, 1, BUFF_SIZE, file)) > 0)
    {
        int result = csv_parse (
            &parser,
            buff,
            read_bytes,
            habit_filter_field_callback,
            habit_filter_line_callback,
            &ctx
        );
        if (result != read_bytes)
        {
            fprintf (
                stderr, "failed to parse csv file: %s\n",
                habit_db_path
            );

            (*alloc.free) (habit_db_path);
            fclose (file);

            exit (EXIT_FAILURE);
        }
    }
    
    csv_fini (
        &parser,
        habit_id_field_callback, 
        habit_id_line_callback,
        &ctx
    );
    csv_free (&parser);
    fclose(file);

    (*alloc.free) (ctx.latest_allocated->next);
    ctx.latest_allocated->next = NULL;
   
    ctx.list = delete_habit_from_list (ctx.list, id, alloc);
    fprintf (stderr, "removed target habit...\n");

    // print_habit_linked_list (*ctx.list);
    persist_habit_list (habit_db_path, ctx.list, alloc);
    
    (*alloc.free) (habit_db_path);
    free_habit_linked_list (ctx.list, ctx.alloc);

    return 0;
}

// --------------------------------------------------------- //

short int update_habit_from_list (
    struct habit_linked_list* list,
    habit val,
    allocator alloc
) {
    short int value_changed = 0;

    struct habit_linked_list* current = list;
    do
    {
        if (current->value.id == val.id)
        {
            copy_habit (
                &current->value,
                val,
                alloc
            );

            value_changed = 1;
        }

        current = current->next;  

    } while (current != NULL);

    return value_changed;
}

int update_habit (store st, habit val, allocator alloc)
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
    
    struct habit_linked_list* item =
        (*alloc.allocate) (sizeof (struct habit_linked_list));

    habit_filter_ctx ctx = {
        .list          = item,
        
        .current_field = 0,
        .current       = item,

        .alloc         = alloc,
    };
    
    char   buff [BUFF_SIZE];
    size_t read_bytes;
    
    while ((read_bytes = fread (buff, 1, BUFF_SIZE, file)) > 0)
    {
        int result = csv_parse (
            &parser,
            buff,
            read_bytes,
            habit_filter_field_callback,
            habit_filter_line_callback,
            &ctx
        );
        if (result != read_bytes)
        {
            fprintf (
                stderr, "failed to parse csv file: %s\n",
                habit_db_path
            );

            (*alloc.free) (habit_db_path);
            fclose (file);

            exit (EXIT_FAILURE);
        }
    }
    
    csv_fini (
        &parser,
        habit_id_field_callback, 
        habit_id_line_callback,
        &ctx
    );
    csv_free (&parser);
    fclose(file);

    (*alloc.free) (ctx.latest_allocated->next);
    ctx.latest_allocated->next = NULL;
   
    update_habit_from_list (ctx.list, val, alloc);

    // print_habit_linked_list (*ctx.list);
    persist_habit_list (habit_db_path, ctx.list, alloc);
    
    (*alloc.free) (habit_db_path);
    free_habit_linked_list (ctx.list, ctx.alloc);

    return 0;
}

// --------------------------------------------------------- //

typedef struct
{
    habit target;

    habit     result;
    short int is_scanning_result;
    short int current_field;

    allocator alloc;

} habit_query_context;

static void query_habit_field_callback (
    void* data,
    size_t len,
    void* out
) {
    habit_query_context* ctx = out;
    
    if (ctx->current_field != 0 && !ctx->is_scanning_result)
    {
        ctx->current_field++;
        return;
    }
    
    if (ctx->current_field == 0)
    {
        int id = (int) strtol (data, NULL, 16);
        if (id != ctx->target.id)
        {
            ctx->current_field++;
            return;
        }
    }

    if (!ctx->is_scanning_result)
        ctx->is_scanning_result = 1;

    if (ctx->alloc.allocate == NULL)
    {
        fprintf (stderr, "failed to get memory allocator\n");

        exit (EXIT_FAILURE);
    }

    switch (ctx->current_field++)
    {
    case 0:
        ctx->result.id = (int) strtol (data, NULL, 16);
        break;

    case 1:
        ctx->result.name = (*ctx->alloc.allocate) (
            strlen (data) + 1
        );
        if (ctx->result.name == NULL)
        {
            fprintf (
                stderr, "failed to allocate name memory!\n"
            );

            exit (EXIT_FAILURE);
        }
        
        strcpy (ctx->result.name, data);
        break;

    case 2:
        ctx->result.type = strtol (data, NULL, 10);
        break;

    case 3:
        ctx->result.metric = (*ctx->alloc.allocate) (
            strlen (data) + 1
        );

        strcpy (ctx->result.metric, data);
        break;

    case 4:
        ctx->result.metric_short =
            (*ctx->alloc.allocate) (strlen (data) + 1);

        strcpy (ctx->result.metric_short, data);
        break;
    
    default:
        // @TODO: handle exception condition
        
        fprintf (
            stderr,
            "habit callback called with undefined field: %d",
            ctx->current_field - 1
        );
    }
}

static void query_habit_line_callback (int _end, void* out)
{
    habit_query_context* ctx = out; 

    if (ctx->is_scanning_result)
        ctx->is_scanning_result = 0; 

    ctx->current_field = 0;
}

habit* query_habit (store st, habit val, allocator alloc)
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
    
    struct habit_linked_list* item =
        (*alloc.allocate) (sizeof (struct habit_linked_list));

    habit_query_context ctx = {
        .target = val,

        .is_scanning_result = 0,
        .current_field      = 0,

        .alloc = alloc,
    };
    
    char   buff [BUFF_SIZE];
    size_t read_bytes;
    
    while ((read_bytes = fread (buff, 1, BUFF_SIZE, file)) > 0)
    {
        int result = csv_parse (
            &parser,
            buff,
            read_bytes,
            query_habit_field_callback,
            query_habit_line_callback,
            &ctx
        );
        if (result != read_bytes)
        {
            fprintf (
                stderr, "failed to parse csv file: %s\n",
                habit_db_path
            );

            (*alloc.free) (habit_db_path);
            fclose (file);

            exit (EXIT_FAILURE);
        }
    }
    
    csv_fini (
        &parser,
        habit_id_field_callback, 
        habit_id_line_callback,
        &ctx
    );

    csv_free      (&parser);
    fclose        (file);
    (*alloc.free) (habit_db_path);

    habit* res = (*alloc.allocate) (sizeof(habit));

    res->id   = ctx.result.id;
    res->type = ctx.result.type;

    res->name = ctx.result.name;
    
    res->metric       = ctx.result.metric;
    res->metric_short = ctx.result.metric_short;

    return res;
}
