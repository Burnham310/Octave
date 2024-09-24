#ifndef UTILS_H_
#define UTILS_H_
#define eprintf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define report(lexer, off, fmt, ...)                                                       \
    do                                                                                     \
    {                                                                                      \
        Loc __loc = off_to_loc(lexer->src, lexer->src_len, off);                           \
        eprintf("%s:%zu:%zu " fmt "\n", lexer->path, __loc.row, __loc.col, ##__VA_ARGS__); \
    } while (0)
// Dynamic Array ---
#define da_free(da)           \
    do                        \
    {                         \
        if ((da).cap > 0)     \
        {                     \
            (da).cap = 0;     \
            free((da.items)); \
        }                     \
    } while (0);
#define da_move(da, dest_slice)    \
    do                             \
    {                              \
        dest_slice.ptr = da.items; \
        dest_slice.len = da.size;  \
        da.cap = 0;                \
        da.size = 0;               \
        da.items = NULL;           \
    } while (0);
#define da_append(da, x)                                                        \
    do                                                                          \
    {                                                                           \
        if ((da).cap == 0)                                                      \
        {                                                                       \
            (da).cap = 10;                                                      \
            (da).items = calloc((da).cap, sizeof((da).items[0]));               \
        }                                                                       \
        if ((da).cap <= (da).size)                                              \
        {                                                                       \
            (da).cap *= 2;                                                      \
            (da).items = realloc((da).items, (da).cap * sizeof((da).items[0])); \
        }                                                                       \
        (da).items[(da).size++] = (x);                                          \
    } while (0)

#define PRINT_WARNING(fmt, ...) \
    fprintf(stderr, "Warning: " fmt "\n", ##__VA_ARGS__)
// ---
#define SliceOf(ty) ty##Slice
#define ArrOf(ty) ty##Arr
#define make_slice(ty) \
    typedef struct     \
    {                  \
        ty *ptr;       \
        size_t len;    \
    } SliceOf(ty)
#define make_arr(ty) \
    typedef struct   \
    {                \
        ty *items;   \
        size_t cap;  \
        size_t size; \
    } ArrOf(ty)
#endif
